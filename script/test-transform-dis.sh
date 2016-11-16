#!/bin/sh
set -xe
barch="$1"
is_thumb=0
if [ "$1" = "thumb" ]; then
    barch=arm
    is_thumb=1
fi
make -j8 out/transform-dis-cases-$1.bin out/test-transform-dis-"$barch"
out/test-transform-dis-"$barch" auto "$is_thumb" < out/transform-dis-cases-$1.bin


