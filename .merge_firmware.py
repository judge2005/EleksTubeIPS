Import("env")
import os

APP_BIN = os.path.join("$BUILD_DIR", "${PROGNAME}.bin")
FS_BIN = os.path.join("$BUILD_DIR", "${ESP32_FS_IMAGE_NAME}.bin")
MERGED_BIN = os.path.join("$BUILD_DIR", "${PROGNAME}_merged.bin")
BOARD_CONFIG = env.BoardConfig()

def merge_bin(source, target, env):
    # The list contains all extra images (bootloader, partitions, eboot) and
    # the final application binary
    flash_images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])) + ["$ESP32_APP_OFFSET", APP_BIN] + ["0x180000", FS_BIN]

    # Run esptool to merge images into a single binary
    env.Execute(
        " ".join(
            [
                "$PYTHONEXE",
                "$OBJCOPY",
                "--chip",
                BOARD_CONFIG.get("build.mcu", "esp32"),
                "merge_bin",
                "--fill-flash-size",
                BOARD_CONFIG.get("upload.flash_size", "4MB"),
                "-o",
                MERGED_BIN,
            ]
            + flash_images
        )
    )

def upload_merged_bin(source, target, env):
    flags=[
        f
        for f in env.get("UPLOADERFLAGS")
        if f not in env.Flatten(env.get("FLASH_EXTRA_IMAGES"))
    ]
    env.Execute(
        " ".join(["$PYTHONEXE","$UPLOADER", " ".join(flags), "0x0", MERGED_BIN])
    )

# Add a post action that runs esptoolpy to merge available flash images
#env.AddPostAction(APP_BIN , merge_bin)

env.AddCustomTarget("merge_bin", [None], [merge_bin])
env.AddCustomTarget("upload_merged_bin", ["merge_bin"], [upload_merged_bin])

# Patch the upload command to flash the merged binary at address 0x0
#env.Replace(
#    UPLOADERFLAGS=[
#            f
#            for f in env.get("UPLOADERFLAGS")
#            if f not in env.Flatten(env.get("FLASH_EXTRA_IMAGES"))
#        ]
#        + ["0x0", MERGED_BIN],
#    UPLOADCMD='"$PYTHONEXE" "$UPLOADER" $UPLOADERFLAGS',
#)
