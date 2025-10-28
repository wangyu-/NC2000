#!/bin/bash

# 清理之前的构建
# rm -rf build

# 使用em++直接编译为ES6模块
echo "Building WASM module with ES6 export..."
em++ \
    main.cpp \
    nc2000.cpp \
    disassembler.cpp \
    cpu_loop.cpp \
    comm.cpp \
    mem.cpp \
    io.cpp \
    rom.cpp \
    nor.cpp \
    nand.cpp \
    ram.cpp \
    dsp/dsp.cpp \
    sound.cpp \
    cmd.cpp \
    udp_server.cpp \
    ansi/w65c02cpu.cpp \
    ansi/w65c02op.cpp \
    NekoDriverIO.cpp \
    key_new.cpp \
    compare/c6502.cpp \
    compare/pc1000bus.cpp \
    cpu_loop_new.cpp \
    settings.cpp \
    key.cpp \
    display.cpp \
    io_new.cpp \
    lcdstripe/json.cpp \
    lcdstripe/lcdpainter.cpp \
    cpu.cpp \
    console.cpp \
    -I. \
    -O3 \
    -g3 \
    -DHANDYPSP \
    -Wno-deprecated-declarations \
    -sUSE_SDL=2 \
    -s MODULARIZE=1 \
    -s EXPORT_ES6=1 \
    -s EXPORT_NAME="WqxsimModule" \
    -s EXPORTED_RUNTIME_METHODS="['FS','cwrap','ccall','callMain']" \
    -s NO_EXIT_RUNTIME=1 \
    -s INVOKE_RUN=0 \
    -sINITIAL_MEMORY=128MB \
    -sALLOW_MEMORY_GROWTH=1 \
    -sASYNCIFY \
    -sUSE_PTHREADS=0 \
    -sENVIRONMENT=web \
    -s WASM=1 \
    --bind \
    -o build/wqxsim.js

echo "Build complete. ES6 module should be available at build/wqxsim.js"
