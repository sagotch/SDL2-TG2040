## Build the SDL for the TG2040 device

```
$ mkdir .TG2040_build
$ cd  .TG2040_build
$ cmake .. -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++
$ make
$ file ./libSDL2-2.0.so.0.2600.5
./libSDL2-2.0.so.0.2600.5: ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV), dynamically linked, BuildID[sha1]=2f8c88ab6b1903e2af821e6ca49c072fb52097c0, not stripped
```
