#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Check out the pinned TalkBack revision and verify the license markers.
# Does not run Gradle and does not copy an APK into the tree.
set -eu
cd "$(dirname "$0")/.."
. ./talkback/PIN

dest=talkback/upstream
mkdir -p "$dest"
if [ ! -d "$dest/.git" ]; then
    git -C "$dest" -c init.defaultBranch=main init
    git -C "$dest" remote add origin "$upstream_url"
fi
git -C "$dest" fetch --depth 1 origin "$revision"
git -C "$dest" checkout --detach FETCH_HEAD

got=$(git -C "$dest" rev-parse HEAD)
if [ "$got" != "$revision" ]; then
    echo "revision mismatch: $got" >&2
    exit 1
fi

sum=$(sha256sum "$dest/LICENSE" | awk '{print $1}')
if [ "$sum" != "$license_sha256" ]; then
    echo "LICENSE sha256 mismatch: $sum" >&2
    exit 1
fi

# Fail closed if the reviewed markers move. Updating the pin includes
# re-reading these files and updating talkback/README.md in the same change.
grep -q 'play-services-mlkit-text-recognition:19.0.1' "$dest/shared.gradle"
grep -q 'com.google.android.play:core:1.10.3' "$dest/shared.gradle"
grep -q 'GNU Lesser General Public License' \
    "$dest/braille/translate/src/phone/jni/third_party/liblouis/liblouis/liblouis.h"
grep -q 'GNU General Public License' \
    "$dest/braille/brltty/src/phone/jni/brlttywrapper/third_party/brltty/Drivers/Braille/VisioBraille/vstp_transfer.c"
grep -q "talkbackVersionName = \"$version_name\"" "$dest/version.gradle"

echo "TalkBack pin verified: $revision ($version_name, $spdx)."
echo "Closed Play artifacts and GPL/LGPL braille headers are still in this revision."
echo "No APK installed."

if [ "${1:-}" = "--install" ]; then
    echo "Refusing to install an APK from the stock phone build." >&2
    exit 1
fi
