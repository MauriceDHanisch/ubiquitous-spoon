# jemalloc-extent-bounds-exact



## Build libc with debug symbols

Rebuild libc with debug symbols using the cheribuild package https://github.com/CTSRD-CHERI/cheribuild. Adapt the commands to your paths.

0. Clean previous builds
```sh
rm -rf ~/cheri/build/
cd ~/cheri/cheribsd/; git clean -xfd
cd ~/cheribuild
```
1. Build cheribsd 
```sh
./cheribuild.py -d cheribsd-morello-purecap
```
2. Start the buildenv
```sh
./cheribuild.py cheribsd-morello-purecap --buildenv
```
3. Remove the libc and rebuild with debug flags
```sh
rm -rf ~/cheri/build/cheribsd-morello-purecap-build/home/maurice/cheri/cheribsd/arm64.aarch64c/lib/libc
bmake -V .OBJDIR
env MK_TESTS=no MK_PROFILE=no MK_LTO=no  CFLAGS+=' -O0 -g -fno-omit-frame-pointer -fno-inline -fno-optimize-sibling-calls'  bmake clean all
```
