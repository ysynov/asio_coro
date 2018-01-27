cmake -DCMAKE_CXX_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-I/home/nds/my/include/c++/v1 -I/home/nds/my/include" -DCMAKE_EXE_LINKER_FLAGS="-L/home/nds/my/lib" ..

cmake -DCMAKE_CXX_COMPILER=clang -D CMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-I/home/nds/my/include/c++/v1 -I/home/nds/my/include" -DCMAKE_EXE_LINKER_FLAGS="-L/home/nds/my/lib" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON  ..

