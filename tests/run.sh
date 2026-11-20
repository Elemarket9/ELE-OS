#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iactivation activation/owner_proof.cpp \
    tests/owner_proof.cpp -lcrypto -o "$tmp/proof"
"$tmp/proof"
"$cxx" -std=c++17 -Wall -Wextra -Werror -pthread -Iactivation activation/*.cpp \
    tests/reactivation.cpp -lcrypto -o "$tmp/session"
mkdir "$tmp/fixture"
"$tmp/session" "$tmp/fixture"
sh tests/status.sh
sh tests/ota.sh
if grep -R -n -i -E 'pixel|shiba' --include='*.h' --include='*.cpp' --include='*.bp' --include='*.sh' --include='*.md' faith tests/faith.cpp; then
    echo "Faith must not name a phone brand or a device codename" >&2
    exit 1
fi
if grep -R -n -E 'ro\.product|ro\.boot' --include='*.h' --include='*.cpp' --include='*.bp' faith tests/faith.cpp; then
    echo "Faith must not branch on device properties" >&2
    exit 1
fi
"$cxx" -std=c++17 -Wall -Wextra -Werror -pthread -Ifaith faith/src/faith.cpp \
    tests/faith.cpp -o "$tmp/faith"
"$tmp/faith"
"$cxx" -std=c++17 -Wall -Wextra -Werror -pthread -Ifaith faith/src/faith.cpp \
    faith/src/faith_text.cpp -o "$tmp/faith-text"
cat > "$tmp/runtime.sh" << 'EOF'
#!/bin/sh
cat >/dev/null
printf '%s' '{"tool":"alarm.set","hour":7,"minute":30,"label":"fixture"}'
EOF
chmod +x "$tmp/runtime.sh"
refused=$("$tmp/faith-text" --runtime "$tmp/runtime.sh" << 'EOF'
set an alarm
EOF
)
printf '%s\n' "$refused" | grep -q '^kind=refused$'
printf '%s\n' "$refused" | grep -q '^reason=disabled$'
if printf '%s\n' "$refused" | grep -q '^action='; then
    echo "a disabled tool must not record an action" >&2
    exit 1
fi
allowed=$("$tmp/faith-text" --enable alarm --runtime "$tmp/runtime.sh" << 'EOF'
set an alarm
EOF
)
printf '%s\n' "$allowed" | grep -q '^kind=emitted$'
printf '%s\n' "$allowed" | grep -q '^action=android.intent.action.SET_ALARM$'
printf '%s\n' "$allowed" | grep -q '^extra.android.intent.extra.alarm.SKIP_UI=false$'
unavailable=$("$tmp/faith-text" << 'EOF'
hello
EOF
)
printf '%s\n' "$unavailable" | grep -q '^kind=unavailable$'
if "$tmp/faith-text" --cloud-enable --cloud-endpoint https://example.com/v1 \
    --cloud-model demo > "$tmp/cloud.out" << 'EOF'
hello
EOF
then
    echo "cloud enable must not send a request" >&2
    exit 1
fi
grep -q 'no request was sent' "$tmp/cloud.out"
if [ -f faith/llm/check.sh ]; then
    sh faith/llm/check.sh
fi
