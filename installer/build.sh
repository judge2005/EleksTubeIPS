#!/usr/bin/env bash
# Builds the web installer site for one release: the install page, the
# ESP Web Tools manifests from web/ and the release's .bin files.
#
# Usage: installer/build.sh <release tag> <output directory>
#
# Needs gh, signed in or with GH_TOKEN set, and jq. The links on the page go to
# this repository, so they're right wherever it's built.

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "Usage: $0 <release tag> <output directory>" >&2
    exit 2
fi

tag=$1
out=$2
version=${tag#v}
root=$(cd "$(dirname "$0")/.." && pwd)
repository=${GITHUB_REPOSITORY:-$(cd "$root" && gh repo view --json nameWithOwner --jq .nameWithOwner)}

rm -rf "$out"
mkdir -p "$out/firmware"

# gh finds the repository from the checkout's remote
(cd "$root" && gh release download "$tag" --pattern '*.bin' --dir "$out/firmware")

for src in "$root"/web/esp-web-tools-manifest-*.json; do
    manifest="$out/firmware/$(basename "$src")"
    sed 's/\$version\$/'"$version"'/g' "$src" > "$manifest"

    # Fail now rather than halfway through someone's install
    for part in $(jq -r '.builds[].parts[].path' "$manifest"); do
        if [ ! -f "$out/firmware/$part" ]; then
            echo "$(basename "$src") needs $part, which isn't in release $tag" >&2
            exit 1
        fi
    done
done

sed -e 's/\$version\$/'"$version"'/g' -e 's|\$repository\$|'"$repository"'|g' \
    "$root/installer/index.html" > "$out/index.html"

echo "Built the installer for $tag in $out"
