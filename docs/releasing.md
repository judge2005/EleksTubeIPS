# Releasing

Pushing a tag builds the release. Publishing the draft it makes releases it.

## Make a release

1. Set the new version in `version.py`. The firmware takes it from there. If `README.md` names the version too, change it there as well. For example, from 1.9.5 to 2.0.0:

   ```
   perl -pi -e 's/\Q1.9.5\E/2.0.0/g' version.py README.md
   ```

   Check the result with `git diff`.

2. Commit, tag and push:

   ```
   git commit -am "Release 2.0.0"
   git tag -a v2.0.0 -m "Release 2.0.0"
   git push origin HEAD v2.0.0
   ```

3. The [Release workflow](../.github/workflows/release.yml) checks that the tag matches `version.py`, then builds every clock from scratch. A few minutes later there's a draft on the repository's Releases page, with all the files. Only people who can push to the repository can see it.

4. Try it. Download the firmware for your clock from the draft and flash it:

   ```
   esptool.py --chip esp32 --port <port> write_flash 0x10000 firmware-elekstubev1-2.0.0.bin
   ```

5. Write what's new in the draft's notes, then choose **Publish release**. That updates the [web installer](../installer/index.html).

A tag with a hyphen, such as `v2.0.0-rc.1`, makes a pre-release, which doesn't replace the web installer. `version.py` has to say `2.0.0-rc.1` too.

## If something goes wrong

* **The tag doesn't match `version.py`.** The workflow stops before building anything. Delete the tag with `git push --delete origin v2.0.0` and `git tag -d v2.0.0`, fix the version, and tag it again.
* **A build fails.** Fix it, delete the tag, and tag the fix. Nothing's published until you publish the draft. If there's already a draft for that tag, its files are replaced, and your notes are kept.
* **A release is already published.** The workflow won't change its files. Make a new version.
* **The web installer didn't update.** Run the [Web installer workflow](../.github/workflows/web-installer.yml) by hand from the Actions tab, and give it the tag.

## One-time setup

The Release and CI workflows need nothing set up. The web installer needs two settings:

1. **Settings > Pages > Build and deployment > Source:** GitHub Actions.
2. **Settings > Environments > github-pages > Deployment branches and tags:** add a tag rule for `v*`. Release runs are on the tag, and otherwise only the default branch can deploy.

## Other things that help

* **Files for a pull request.** Every CI run has a `release-files` artifact with the same files a release would have, so a change can be flashed before it's merged.
* **Release files on your own computer.** `pio run -e elekstubev1 -t buildfs && pio run -e elekstubev1 -t release` puts a clock's files in `releases/`, and `python3 tools/check_release.py --files releases` checks them once you've built every clock.
* **Decoding a crash.** Each release has `firmware-symbols-<version>.zip`. A release built by CI isn't byte for byte what your own build makes, so a backtrace needs the symbols from the release it came from:

   ```
   xtensa-esp32-elf-addr2line -pfiaC -e firmware-elekstubev1-2.0.0.elf <addresses from the backtrace>
   ```
