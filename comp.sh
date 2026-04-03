#!/bin/bash

# uv -vv pip install -e ".[dev]" --no-progress -Cbuild.verbose=true
#
cmake -S . -B debug \
	-DCMAKE_BUILD_TYPE=Release \
	-Dmyproject_ENABLE_SANITIZER_ADDRESS=OFF \
	-Dmyproject_ENABLE_SANITIZER_UNDEFINED=OFF \
	-Dmyproject_ENABLE_CLANG_TIDY=OFF \
	-Dmyproject_ENABLE_CPPCHECK=OFF \

cmake --build debug --target watpocket --parallel 12
