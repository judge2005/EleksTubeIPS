import os
import shutil
import re
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
    src = os.path.join(env["PROJECT_DIR"], env.GetBuildPath("$BUILD_DIR"), env.GetBuildPath("${PROGNAME}.bin"))
    dst = os.path.join(env["PROJECT_DIR"], 'releases', "firmware-" + env["PIOENV"] + "-" + version + ".bin")
    #print(src + " " + dst)
    shutil.copy(src, dst)

def copy_fs_to_release(*args, **kwargs):
    src = os.path.join(env["PROJECT_DIR"], env.GetBuildPath("$BUILD_DIR"), env.GetBuildPath("${ESP32_FS_IMAGE_NAME}.bin"))
    dst = os.path.join(env["PROJECT_DIR"], 'releases', env.GetBuildPath("$ESP32_FS_IMAGE_NAME") + "-" + version + ".bin")
    #print(src + " " + dst)
    shutil.copy(src, dst)

env.AddCustomTarget("release", [os.path.join("$BUILD_DIR", "${PROGNAME}.bin"), os.path.join("$BUILD_DIR", "${ESP32_FS_IMAGE_NAME}.bin")], [copy_firmware_to_release, copy_fs_to_release, copy_manifests_to_release])

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