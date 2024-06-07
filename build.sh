#!/bin/bash

mkdir "build_dir"
cd "build_dir"
cmake -DCMAKE_BUILD_TYPE=Release ..
make
cd -