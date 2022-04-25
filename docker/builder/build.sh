#!/bin/bash

rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake mpeg-ts-client
cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" .
make -j3
rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake
