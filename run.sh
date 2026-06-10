#!/bin/sh
set -e
./compile.sh
target="$1"
shift
exec "./build/$target" "$@"
