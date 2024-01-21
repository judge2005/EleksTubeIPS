#ifndef _IPS_IMAGE_UNPACKER_H
#define _IPS_IMAGE_UNPACKER_H
#include <ESP32-targz.h>

class ImageUnpacker {
public:
    void unpackImages(const String &faceName, const String &dest);

protected:
    static bool newUnpack;
    static const char* unpackName;

    static TarGzUnpacker& getUnpacker();

    static void statusProgressCallback(const char* name, size_t size, size_t total_unpacked);
    static void unpackProgressCallback(uint8_t progress);
};

#endif