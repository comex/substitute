#!/bin/sh
set -xe
make out/transform-dis-cases-$1.bin out/test-transform-dis-$1
out/test-transform-dis-$1 auto < out/transform-dis-cases-$1.bin


