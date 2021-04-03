mkdir build\
set PREFIX=C:/
set CCFLAGS=-IC:/include
set LDFLAGS=-LC:/lib
meson setup build --buildtype=debug --prefix C:/
cd build
ninja
cd ..