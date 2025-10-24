cmake_minimum_required(VERSION 3.15)
project(wqxsim LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "-O3 -g3 -pthread -DHANDYPSP -Wno-deprecated-declarations")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (EMSCRIPTEN)
    message(STATUS "Building for WebAssembly via Emscripten")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1")
endif()

add_executable(wqxsim
    main.cpp
    nc2000.cpp
    disassembler.cpp
    cpu_loop.cpp
    comm.cpp
    mem.cpp
    io.cpp
    rom.cpp
    nor.cpp
    nand.cpp
    ram.cpp
    dsp/dsp.cpp
    sound.cpp
    cmd.cpp
    udp_server.cpp
    ansi/w65c02cpu.cpp
    ansi/w65c02op.cpp
    NekoDriverIO.cpp
    key_new.cpp
    compare/c6502.cpp
    compare/pc1000bus.cpp
    cpu_loop_new.cpp
    settings.cpp
    key.cpp
    display.cpp
    io_new.cpp
    lcdstripe/json.cpp
    lcdstripe/lcdpainter.cpp
    cpu.cpp
    console.cpp
)

if (EMSCRIPTEN)
    set_target_properties(wqxsim PROPERTIES LINK_FLAGS "\
        -sUSE_SDL=2 \
        -s INITIAL_MEMORY=128MB \
        -sALLOW_MEMORY_GROWTH=1 \
        -sASYNCIFY \
        -s USE_PTHREADS=0 \
        -DHAVE_HAPTIC=0 \
        -sEXPORTED_RUNTIME_METHODS=['FS','cwrap','ccall'] \
        --preload-file roms@/roms \
        --preload-file resource@/resource \
    ")
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
    target_link_libraries(wqxsim ${SDL2_LIBRARIES})
    if(WIN32)
        target_link_libraries(wqxsim wsock32 ws2_32)
    endif()
endif()
