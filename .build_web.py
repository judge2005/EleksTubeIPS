from shutil import copyfile
from subprocess import check_output, CalledProcessError
import sys
import os
import platform
import subprocess

Import("env")

def is_tool(name):
    cmd = "where" if platform.system() == "Windows" else "which"
    try:
        check_output([cmd, name])
        return True
    except:
        return False;

# Elsewhere an old build's pages can stand in, with a warning. On a CI server, which has no old build and
# whose files get released, it has to stop the build.
def failed():
    print("WARNING: Failed to build web package. Using pre-built page.")
    if os.environ.get("CI"):
        env.Exit(1)

def build_web():
    if not is_tool("npm"):
        print("npm wasn't found")
        failed()
    else:
        os.chdir("web")
        print("Attempting to build webpage...")
        try:
            if platform.system() == "Windows":
                print(check_output(["npm.cmd", "install", "--only=dev"]))
                print(check_output(["node_modules\\.bin\\gulp.cmd"]))
            else:
                # In CI, exactly what package-lock.json says, without running anything from the packages
                print(check_output(["npm", "ci", "--ignore-scripts"] if os.environ.get("CI") else ["npm", "install"]))
                print(check_output(["node_modules/.bin/gulp"]))
        except OSError as e:
            print("Encountered error OSError building webpage:", e)
            if e.filename:
                print("Filename is", e.filename)
            failed()
        except CalledProcessError as e:
            print(e.output)
            print("Encountered error CalledProcessError building webpage:", e)
            failed()
        except Exception as e:
            print("Encountered error", type(e).__name__, "building webpage:", e)
            failed()
        finally:
            os.chdir("..");

build_web()