import os
import shutil
import re
import json
from version import *

Import("env")

def read_version():
    src = os.path.join(env["PROJECT_DIR"], "version.txt")

def copy_and_replace(src, dest):
    with open(src, "r") as src_file:
        #read the file contents
        file_contents = src_file.read()
        text_pattern = re.compile(re.escape("$version$"), 0)
        file_contents = text_pattern.sub(version, file_contents)
        with open(dest, "w") as dest_file:
            dest_file.seek(0)
            dest_file.truncate()
            dest_file.write(file_contents)

def copy_manifests_to_release(*args, **kwargs):
    src = os.path.join(env["PROJECT_DIR"], "web", "esp-web-tools-manifest-" + env["PIOENV"] + "-firmware-only.json")
    dst = os.path.join(env["PROJECT_DIR"], 'releases', "esp-web-tools-manifest-" + env["PIOENV"] + "-firmware-only.json")
    copy_and_replace(src, dst)
    src = os.path.join(env["PROJECT_DIR"], "web", "esp-web-tools-manifest-" + env["PIOENV"] + ".json")
    dst = os.path.join(env["PROJECT_DIR"], 'releases', "esp-web-tools-manifest-" + env["PIOENV"] + ".json")
    copy_and_replace(src, dst)

def copy_firmware_to_release(*args, **kwargs):
    # Open and read the manifest
    manifest_path = os.path.join(env["PROJECT_DIR"], 'releases', "esp-web-tools-manifest-" + env["PIOENV"] + ".json")
    with open(manifest_path, 'r') as file:
        manifest = json.load(file)

    #bootloader
    src = os.path.join(env["PROJECT_DIR"], env.GetBuildPath("$BUILD_DIR"), env.GetBuildPath("bootloader.bin"))
    dst = os.path.join(env["PROJECT_DIR"], 'releases', manifest["builds"][0]["parts"][0]["path"])
    print(src + " " + dst)
    shutil.copy(src, dst)

    #partitions
    src = os.path.join(env["PROJECT_DIR"], env.GetBuildPath("$BUILD_DIR"), env.GetBuildPath("partitions.bin"))
    dst = os.path.join(env["PROJECT_DIR"], 'releases', manifest["builds"][0]["parts"][1]["path"])
    print(src + " " + dst)
    shutil.copy(src, dst)

    # firmware
    src = os.path.join(env["PROJECT_DIR"], env.GetBuildPath("$BUILD_DIR"), env.GetBuildPath("${PROGNAME}.bin"))
    dst = os.path.join(env["PROJECT_DIR"], 'releases', manifest["builds"][0]["parts"][3]["path"])
    print(src + " " + dst)
    shutil.copy(src, dst)

def copy_fs_to_release(*args, **kwargs):
    # Open and read the manifest
    manifest_path = os.path.join(env["PROJECT_DIR"], 'releases', "esp-web-tools-manifest-" + env["PIOENV"] + ".json")
    with open(manifest_path, 'r') as file:
        manifest = json.load(file)

    src = os.path.join(env["PROJECT_DIR"], env.GetBuildPath("$BUILD_DIR"), env.GetBuildPath("${ESP32_FS_IMAGE_NAME}.bin"))
    dst = os.path.join(env["PROJECT_DIR"], 'releases', manifest["builds"][0]["parts"][4]["path"])
    print(src + " " + dst)
    shutil.copy(src, dst)

env.AddCustomTarget("release", [os.path.join("$BUILD_DIR", "${PROGNAME}.bin"), os.path.join("$BUILD_DIR", "${ESP32_FS_IMAGE_NAME}.bin")], [copy_manifests_to_release, copy_fs_to_release, copy_firmware_to_release])

def list_fs_contents(*args, **kwargs):
    # Open and read the manifest
    src = os.path.join(env["PROJECT_DIR"], env.GetBuildPath("$BUILD_DIR"), env.GetBuildPath("${ESP32_FS_IMAGE_NAME}.bin"))

    # Run esptool to merge images into a single binary
    env.Execute(
        " ".join(
            [
                "/Users/Nemesis/.platformio/packages/tool-mklittlefs/mklittlefs",
                "-l",
                src
            ]
        )
    )

env.AddCustomTarget(
    "listfs",
    None,
    list_fs_contents
)

env.AddCustomTarget(
    name="release_prologue",
    dependencies=None,
    actions=[
        "pio --version",
        "python --version"
    ],
    title="Prepare for Release",
    description="Prepare for release"
)