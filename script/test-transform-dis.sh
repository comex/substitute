#!/bin/sh
set -xe
make -j8 out/transform-dis-cases-$1.bin out/test-transform-dis-$1
out/test-transform-dis-$1 auto < out/transform-dis-cases-$1.bin


