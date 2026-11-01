#include <LittleFS.h>

#include "TFTs.h"
#include "ImageUnpacker.h"

extern void fsChanged();

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

const String& ImageUnpacker::unpackImages(const String &srcDir, const String &destDir, const String &newFaces, const String &oldFaces) {
    if (unpackImages(srcDir + newFaces, destDir)) {
        return newFaces;
    } else {
        tfts->setStatus("Reverting!");
        unpackImages(srcDir + oldFaces, destDir);
        return oldFaces;
    }
}

bool ImageUnpacker::unpackImages(const String &faceName, const String &dest) {
	String fileName(faceName + ".tar.gz");

    if (LittleFS.exists(fileName)) {
        newUnpack = true;

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

        if( !unpacked ) {
            Serial.printf("tarGzExpander+intermediate file failed with return code #%d\n", unpacker.tarGzGetError() );
            return false;
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
        return true;
    }

    return false;
}
