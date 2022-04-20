#/usr/bin/env bash

cur_path="$(dirname "$(readlink -f "$0")")"
cd $cur_path

mkdir build_aarch64
cd build_aarch64

cmake .. -DPLATFORM=aarch64

cmake --build .