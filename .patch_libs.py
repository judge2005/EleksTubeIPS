Import("env")
import os
import subprocess
import sys

# Fixes for libraries that haven't been released by their maintainers. Each patch is applied with
# git apply to this environment's copy of the library before it's built. One that's already applied
# is left alone. One that no longer fits stops the build, because the library has changed under it.
PATCHES = {
    # When the weather fetch left the clock short of memory, pages and files being sent lost data and
    # never finished, and a new connection could abort the program
    "ESP Async WebServer": "patches/ESPAsyncWebServer.patch",
}

def git_apply(library, patch, *options):
    # Stop git looking above the library, so it can't take the project for the library's repository
    environment = dict(os.environ, GIT_CEILING_DIRECTORIES=os.path.dirname(library))
    return subprocess.run(["git", "apply", *options, patch], cwd=library, env=environment,
                          capture_output=True, text=True)

for name, patch in PATCHES.items():
    library = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"), name)
    patch = os.path.join(env.subst("$PROJECT_DIR"), patch)

    if not os.path.isdir(library):
        continue    # Not installed yet, as when cleaning a fresh clone. A build installs it before this runs.

    if git_apply(library, patch, "--reverse", "--check").returncode == 0:
        continue    # Already applied

    result = git_apply(library, patch)
    if result.returncode != 0:
        sys.stderr.write("Error: %s doesn't apply to %s:\n%s" % (patch, library, result.stderr))
        env.Exit(1)

    print("Patched %s with %s" % (name, os.path.basename(patch)))
