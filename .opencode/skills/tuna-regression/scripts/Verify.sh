#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../../../.." && pwd)
stamp=$(date -u +%Y%m%dT%H%M%SZ)
run="$root/artifacts/$stamp-$$"
mkdir -p "$run"
status=0
if make -C "$root" "$@" > "$run/build.log" 2>&1; then
    passed=true
else
    status=$?
    passed=false
fi
cat "$run/build.log"
revision=$(git -C "$root" rev-parse HEAD 2>/dev/null || printf unknown)
printf '{"passed":%s,"exitCode":%s,"revision":"%s","utc":"%s","emulatorAcceptance":"not run","hardwareAcceptance":"not run"}\n' \
    "$passed" "$status" "$revision" "$stamp" > "$run/result.json"
printf 'Evidence: %s\n' "$run"
exit "$status"
