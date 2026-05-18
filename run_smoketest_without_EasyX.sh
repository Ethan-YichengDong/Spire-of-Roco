#!/bin/bash

echo "Cleaning previous objects..."
rm -f *.o mac_smoketest

echo "Compiling Core Engine (C)..."
gcc -Wall -Wextra -c src_data/*.c src_engine/*.c src_ai/*.c

echo "Compiling Mac CLI Manager (C++)..."
# 注意：我们这里不编译 src_gui/*.cpp (包含Windows专用的GUI逻辑)，而是编译替代它的 mac_cli_manager.cpp
g++ -Wall -Wextra -c src_smoketest/mac_cli_manager.cpp

echo "Linking everything..."
g++ *.o -o mac_smoketest

echo "Compilation success. Running Smoke Test..."
./mac_smoketest
