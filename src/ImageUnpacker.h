#ifndef _IPS_IMAGE_UNPACKER_H
#define _IPS_IMAGE_UNPACKER_H
#include <ESP32-targz.h>

class ImageUnpacker {
public:
    const String& unpackImages(const String &srcDir, const String &destDir, const String &newFaces, const String &oldFaces);

    // Whether name can be used for <name>.tar.gz in one of the /ips directories
    static bool isValidName(const String &name);

protected:
    int unpackImages(const String &fileName, const String &dest);

    static bool newUnpack;
    static const char* unpackName;

    static TarGzUnpacker& getUnpacker();

    static void statusProgressCallback(const char* name, size_t size, size_t total_unpacked);
    static void unpackProgressCallback(uint8_t progress);
};

#endif