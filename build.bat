@echo off
IF NOT EXIST build (mkdir build)
clang -g code/main.c -I include -I include/SDL3 lib/Windows/x64/SDL3.lib -o build/game.exe
REM clang -g ../code/main.c -Wl,/SUBSYSTEM:WINDOWS -I ../include -I ../include/SDL3 ../lib/Windows/x64/SDL3.lib -o game.exe
