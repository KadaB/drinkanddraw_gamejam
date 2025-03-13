@echo off
pushd build
clang -g ../code/main.c -I ../include -I ../include/SDL3 ../lib/Windows/x64/SDL3.lib -o game.exe
REM clang -g ../code/main.c -Wl,/SUBSYSTEM:WINDOWS -I ../include -I ../include/SDL3 ../lib/Windows/x64/SDL3.lib -o game.exe
popd
