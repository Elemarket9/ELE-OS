#!/bin/sh
# Compile and package the built-in ELE Share APK against the public Android SDK.
# This proves source/resource packaging only, not device transfer performance.
set -eu
cd "$(dirname "$0")/.."
sdk=${ANDROID_HOME:?set ANDROID_HOME to an Android SDK}
api=${ELE_API:-36}
jar="$sdk/platforms/android-$api/android.jar"
[ -f "$jar" ] || { echo "Missing $jar" >&2; exit 1; }
bt=${ELE_BUILD_TOOLS:-$(ls -d "$sdk"/build-tools/* | sort -V | tail -1)}
out=${1:-$(mktemp -d)}
mkdir -p "$out"
rm -rf "$out/gen" "$out/classes" "$out/dex" "$out"/*.apk
app=share
"$bt/aapt2" compile --pseudo-localize --dir "$app/res" -o "$out/res.zip"
mkdir -p "$out/gen"
"$bt/aapt2" link -I "$jar" --manifest "$app/AndroidManifest.xml" \
    --min-sdk-version 35 --target-sdk-version "$api" --java "$out/gen" \
    -o "$out/base.apk" "$out/res.zip"
mkdir -p "$out/classes"
find "$app/src" "$app/app/src" "$app/protocol" "$out/gen" -name '*.java' > "$out/sources"
javac -source 17 -target 17 -Xlint:all,-options -Werror -encoding UTF-8 \
    -classpath "$jar" -d "$out/classes" @"$out/sources"
mkdir -p "$out/dex"
find "$out/classes" -name '*.class' > "$out/classfiles"
"$bt/d8" --min-api 35 --lib "$jar" --output "$out/dex" @"$out/classfiles"
cp "$out/base.apk" "$out/unaligned.apk"
(cd "$out/dex" && zip -q -X "../unaligned.apk" classes.dex)
"$bt/zipalign" -f -p 4 "$out/unaligned.apk" "$out/aligned.apk"
key="$out/test-only.keystore"
[ -f "$key" ] || keytool -genkeypair -keystore "$key" -storepass android \
    -keypass android -alias debug -keyalg RSA -keysize 2048 -validity 30 \
    -dname 'CN=ELE-OS share test only' > /dev/null 2>&1
"$bt/apksigner" sign --ks "$key" --ks-pass pass:android --key-pass pass:android \
    --out "$out/EleShare.apk" "$out/aligned.apk"
"$bt/apksigner" verify "$out/EleShare.apk"
"$bt/aapt2" dump xmltree --file AndroidManifest.xml "$out/EleShare.apk" > "$out/manifest.txt"
grep -q 'android.intent.action.SEND' "$out/manifest.txt"
grep -q 'android.intent.action.SEND_MULTIPLE' "$out/manifest.txt"
grep -q 'NEARBY_WIFI_DEVICES' "$out/manifest.txt"
echo "Built $out/EleShare.apk (compile/package verified; device transfer not tested)"
