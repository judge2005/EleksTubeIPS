Import("env")
import os
import subprocess
import sys

# Fixes for libraries that haven't been released by their maintainers. Each patch is applied with
# git apply to this environment's copy of the library before it's built. One that's already applied
# is left alone. If an earlier version of a patch is applied, the files it changes are put back
# first. One that still doesn't fit stops the build, because the library has changed under it.
PATCHES = {
    # When the weather fetch left the clock short of memory, pages and files being sent lost data and
    # never finished, and a new connection could abort the program. Dropping the headers no handler asked
    # for read memory it had just freed.
    "ESP Async WebServer": "patches/ESPAsyncWebServer.patch",
    # A face, icon set or slide show that ran out of space partway through unpacking was reported as
    # unpacked, with the rest of its files left out. Too little memory in one piece for its buffers
    # aborted the program, and now the clock can lend it them.
    "ESP32-targz": "patches/ESP32-targz.patch",
    # A web connection that came in when there wasn't the memory for it was freed, then used by lwIP,
    # which stopped the program with "tcp_receive: wrong state"
    "AsyncTCP": "patches/AsyncTCP.patch",
}

def git(library, *args):
    # Stop git looking above the library, so it can't take the project for the library's repository
    environment = dict(os.environ, GIT_CEILING_DIRECTORIES=os.path.dirname(library))
    return subprocess.run(["git", *args], cwd=library, env=environment, capture_output=True, text=True)

for name, patch in PATCHES.items():
    library = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"), name)
    patch = os.path.join(env.subst("$PROJECT_DIR"), patch)

    if not os.path.isdir(library):
        continue    # Not installed yet, as when cleaning a fresh clone. A build installs it before this runs.

    if git(library, "apply", "--reverse", "--check", patch).returncode == 0:
        continue    # Already applied

    if git(library, "apply", "--check", patch).returncode != 0 and os.path.isdir(os.path.join(library, ".git")):
        # Most likely an earlier version of the patch, so put back the files this one changes.
        # --numstat lists them, as lines of lines added, lines deleted and path.
        files = [line.split("\t")[2] for line in git(library, "apply", "--numstat", patch).stdout.splitlines()]
        git(library, "checkout", "--", *files)

    result = git(library, "apply", patch)
    if result.returncode != 0:
        sys.stderr.write("Error: %s doesn't apply to %s:\n%s" % (patch, library, result.stderr))
        env.Exit(1)

    print("Patched %s with %s" % (name, os.path.basename(patch)))
