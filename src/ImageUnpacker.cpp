#include <LittleFS.h>

#include "TFTs.h"
#include "ImageUnpacker.h"

extern void fsChanged();
extern size_t fsFreeBytes();
extern void broadcastUpdate(String msg);
extern volatile bool displayBufferLent;

// LittleFS hands out space in blocks of this size, a flash sector
static const size_t blockSize = 4096;

bool ImageUnpacker::newUnpack = true;
const char* ImageUnpacker::unpackName = "";

bool ImageUnpacker::isValidName(const String &name) {
    // Names are stored in config items that hold at most 25 characters, and end up in file
    // paths and in JSON that the web UI builds by hand
    if (name.length() == 0 || name.length() > 25) {
        return false;
    }

    for (int i = 0; i < name.length(); i++) {
        char c = name[i];
        if (c < ' ' || c > '~' || c == '/' || c == '\\' || c == '"') {
            return false;
        }
    }

    return true;
}

TarGzUnpacker &ImageUnpacker::getUnpacker() {
    static TarGzUnpacker *unpacker = 0;

    if (unpacker == 0) {
        unpacker = new TarGzUnpacker();
        unpacker->setTarVerify( true ); // true = enables health checks but slows down the overall process
        unpacker->setupFSCallbacks( targzTotalBytesFn, targzFreeBytesFn ); // prevent the partition from exploding, recommended
        unpacker->setGzProgressCallback( unpackProgressCallback ); // targzNullProgressCallback or defaultProgressCallback
        unpacker->setLoggerCallback( BaseUnpacker::targzPrintLoggerCallback  );    // gz log verbosity
        unpacker->setTarStatusProgressCallback( statusProgressCallback ); // print the filenames as they're expanded
        unpacker->setTarMessageCallback( BaseUnpacker::targzPrintLoggerCallback ); // tar log verbosity

        // The library's 36 KB of buffers come from the display buffer, as serving web pages can leave
        // the heap without that much in one piece. Taking it from the heap also left too little for
        // Wi-Fi. Unpacking is done by updateDisplays(), holding memMutex, so nothing draws or fetches
        // the weather with the display buffer meanwhile.
        static_assert(sizeof(StaticSprite::output_buffer) >= GZIP_DICT_SIZE + GZIP_BUFF_SIZE + 1, "The display buffer is too small");
        unpacker->setBuffers( StaticSprite::output_buffer );
    }

    return *unpacker;
}

void ImageUnpacker::statusProgressCallback(const char* name, size_t size, size_t total_unpacked) {
	unpackName = name;
}

void ImageUnpacker::unpackProgressCallback(uint8_t progress) {
	tfts->drawMeter(progress, newUnpack, unpackName);
	newUnpack = false;
}

// The size of the tar in a .tar.gz file, from the end of the file. 0 if it isn't a gzip file.
static size_t tarSize(const String &fileName) {
    fs::File file = LittleFS.open(fileName);
    uint8_t magic[2], size[4];

    if (!file || file.size() < 18 || file.read(magic, 2) != 2 || magic[0] != 0x1f || magic[1] != 0x8b
        || !file.seek(file.size() - 4) || file.read(size, 4) != 4) {
        return 0;
    }

    return size[0] | size[1] << 8 | size[2] << 16 | (size_t)size[3] << 24;
}

// The space the files in dir take up, and give back when they're removed
static size_t spaceUsed(const String &dir) {
    size_t used = 0;

    fs::File d = LittleFS.open(dir);
    if (d && d.isDirectory()) {
        fs::File file = d.openNextFile();
        while (file) {
            used += (file.size() + blockSize - 1) / blockSize * blockSize;
            file = d.openNextFile();
        }
    }

    return used;
}

// Whether the unpack failed because the filesystem is full. It fails to create or write a file for
// no other reason.
static bool outOfSpace(int error) {
    switch (error) {
        case ESP32_TARGZ_UZLIB_INVALID_FILE:    // From the library's check of the whole set, as tarSize() found a gzip file
        case ESP32_TARGZ_FS_FULL_ERROR:         // From its check of each file
        case ESP32_TARGZ_FS_ERROR:              // A file couldn't be created
        case ESP32_TARGZ_FS_WRITE_ERROR:        // Or written
        case ESP32_TARGZ_TAR_ERR_FOOTERCB_FAIL: // Or came out short
            return true;
        default:
            return false;
    }
}

static bool shortOfMemory(int error) {
    return error == ESP32_TARGZ_HEAP_TOO_LOW || error == ESP32_TARGZ_UZLIB_MALLOC_FAIL || error == ESP32_TARGZ_TAR_ERR_HEAP_TOO_LOW;
}

// The library won't start with less memory free than its buffers take, though they're lent to it, see
// getUnpacker(). Serving web pages can leave too little for a moment, so this waits up to 10 seconds.
static bool waitForMemory() {
    for (int i = 0; i < 20; i++) {
        if (ESP.getFreeHeap() >= GZIP_DICT_SIZE + GZIP_BUFF_SIZE) {
            return true;
        }
        delay(500);
    }

    return false;
}

// Tells the web UI, which shows it on the Files page. An empty message clears it.
static void showError(const String &message) {
    // The web UI shows it as HTML, after putting it in a jQuery selector in single quotes. Names can't
    // have double quotes or backslashes, see isValidName(), so the JSON is fine.
    String html(message);
    html.replace("&", "&amp;");
    html.replace("<", "&lt;");
    html.replace(">", "&gt;");
    html.replace("'", "&#39;");

    broadcastUpdate("{\"type\":\"sv.update\",\"value\":{\"file_error\":\"" + html + "\"}}");
}

static void failed(const String &name, int error) {
    if (outOfSpace(error)) {
        tfts->setStatus("Not enough space!");
        showError("Not enough space for " + name + ". Delete sets you don't use, then try again.");
    } else if (shortOfMemory(error)) {
        tfts->setStatus("Reverting!");
        showError("The clock was too busy to unpack " + name + ". Try again.");
    } else {
        tfts->setStatus("Reverting!");
        showError(name + " couldn't be unpacked. Try uploading it again.");
    }
}

const String& ImageUnpacker::unpackImages(const String &srcDir, const String &destDir, const String &newFaces, const String &oldFaces) {
    String fileName(srcDir + newFaces + ".tar.gz");

    // Unpacking needs at least the size of the tar, and the files unpacked now give their space back.
    // Checking before they're removed keeps them if the new set can't fit. Each file takes whole
    // blocks, and the tar doesn't say how many it holds, so a set can pass this and still not fit.
    // That's caught as it's unpacked, see unpackImages() below.
    size_t needed = tarSize(fileName);
    if (needed == 0) {
        failed(newFaces, ESP32_TARGZ_UZLIB_PARSE_HEADER_FAILED);
        return oldFaces;
    } else if (needed > fsFreeBytes() + spaceUsed(destDir)) {
        failed(newFaces, ESP32_TARGZ_FS_FULL_ERROR);
        return oldFaces;
    } else if (!waitForMemory()) {
        failed(newFaces, ESP32_TARGZ_HEAP_TOO_LOW);
        return oldFaces;
    }

    int error = unpackImages(fileName, destDir);
    if (error == ESP32_TARGZ_OK) {
        showError("");
        return newFaces;
    }

    failed(newFaces, error);
    if (newFaces != oldFaces) {     // Otherwise the set in use was replaced, and the old one has gone
        unpackImages(srcDir + oldFaces + ".tar.gz", destDir);
    }
    return oldFaces;
}

// Replaces the files in dest with those in fileName. Returns ESP32_TARGZ_OK, or why it couldn't.
int ImageUnpacker::unpackImages(const String &fileName, const String &dest) {
    if (LittleFS.exists(fileName)) {
        newUnpack = true;
        waitForMemory();    // Again, as the web server may have taken it since, and for going back

        fs::File dir = LittleFS.open(dest);
        String name = dir.getNextFileName();
        while(name.length() > 0){
            LittleFS.remove(name);
            name = dir.getNextFileName();
        }
        dir.close();

        TarGzUnpacker &unpacker = getUnpacker();

        bool unpacked = unpacker.tarGzExpander(tarGzFS, fileName.c_str(), tarGzFS, dest.c_str(), nullptr );
        fsChanged();    // Files were removed, and maybe added, whether or not that worked
        displayBufferLent = true;   // So everything is drawn again

        if( !unpacked ) {
            Serial.printf("tarGzExpander+intermediate file failed with return code #%d\n", unpacker.tarGzGetError() );
            return unpacker.tarGzGetError();
        } else {
            Serial.println("File unzipped");
        }

#ifdef notdef
        dir = LittleFS.open(dest);
        File file = dir.openNextFile();
        while(file){
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");

    #ifdef CONFIG_LITTLEFS_FOR_IDF_3_2
            Serial.println(file.size());
    #else
            Serial.print(file.size());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
    #endif
            file.close();
            file = dir.openNextFile();
        }
        dir.close();
#endif
        return ESP32_TARGZ_OK;
    }

    return ESP32_TARGZ_STREAM_ERROR;  // As the library has it
}
