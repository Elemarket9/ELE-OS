#!/bin/sh
# Builds the External devices app with the public Android SDK, outside AOSP,
# and checks the packaged resources. This proves source and resource
# compilation only; it does not install or run the app.
#
# Requires: ANDROID_HOME with platforms;android-36 and a build-tools release,
# a JDK 17+ (javac, keytool). Usage: sh tests/peripherals-sdk.sh [OUT_DIR]
set -eu
cd "$(dirname "$0")/.."
sdk=${ANDROID_HOME:?set ANDROID_HOME to an Android SDK}
api=${ELE_API:-36}
jar="$sdk/platforms/android-$api/android.jar"
[ -f "$jar" ] || { echo "Missing $jar" >&2; exit 1; }
bt=${ELE_BUILD_TOOLS:-$(ls -d "$sdk"/build-tools/* | sort -V | tail -1)}
out=${1:-$(mktemp -d)}
mkdir -p "$out"
rm -rf "$out/gen" "$out/classes" "$out/dex" "$out"/*.apk "$out"/*.zip
app=peripherals

# Soong always compiles app resources with --pseudo-localize (en-XA, ar-XB).
"$bt/aapt2" compile --pseudo-localize --dir "$app/res" -o "$out/res.zip"
mkdir -p "$out/gen"
"$bt/aapt2" link -I "$jar" --manifest "$app/AndroidManifest.xml" \
    --min-sdk-version 35 --target-sdk-version "$api" --java "$out/gen" \
    -o "$out/base.apk" "$out/res.zip"

mkdir -p "$out/classes"
find "$app/src" "$out/gen" -name '*.java' > "$out/sources"
javac -source 17 -target 17 -Xlint:all,-options -Werror -encoding UTF-8 \
    -classpath "$jar" -d "$out/classes" @"$out/sources"

mkdir -p "$out/dex"
find "$out/classes" -name '*.class' > "$out/classfiles"
"$bt/d8" --min-api 35 --lib "$jar" --output "$out/dex" @"$out/classfiles"
cp "$out/base.apk" "$out/unaligned.apk"
(cd "$out/dex" && zip -q -X "../unaligned.apk" classes.dex)
"$bt/zipalign" -f -p 4 "$out/unaligned.apk" "$out/aligned.apk"

key="$out/debug.keystore"
[ -f "$key" ] || keytool -genkeypair -keystore "$key" -storepass android \
    -keypass android -alias debug -keyalg RSA -keysize 2048 -validity 30 \
    -dname 'CN=ELE-OS test only' > /dev/null 2>&1
"$bt/apksigner" sign --ks "$key" --ks-pass pass:android --key-pass pass:android \
    --out "$out/ElePeripherals.apk" "$out/aligned.apk"
"$bt/apksigner" verify "$out/ElePeripherals.apk"

# Packaged resource checks.
"$bt/aapt2" dump resources "$out/ElePeripherals.apk" > "$out/resources.txt"
"$bt/aapt2" dump xmltree --file AndroidManifest.xml "$out/ElePeripherals.apk" > "$out/manifest.txt"
grep -q 'supportsRtl.*=true' "$out/manifest.txt"
grep -Eq 'label.*=@0x7f' "$out/manifest.txt"
keys=$(grep -o '<string name="[a-z_]*"' "$app/res/values/strings.xml" | cut -d'"' -f2)
for key in $keys; do
    entry=$(awk -v k="string/$key" '$1=="resource" && $3==k {f=1; next} f && $1=="resource" {exit} f' "$out/resources.txt")
    for config in '()' '(en-rXA)' '(ar-rXB)'; do
        printf '%s\n' "$entry" | grep -Fq "$config " || {
            echo "string/$key lacks $config" >&2; exit 1; }
    done
done
count=$(printf '%s\n' $keys | wc -l)
echo "Built $out/ElePeripherals.apk: $count strings, each with default, en-XA and ar-XB"
