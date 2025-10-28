cmake_minimum_required(VERSION 3.15)
project(wqxsim LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "-O3 -g3 -pthread -DHANDYPSP -Wno-deprecated-declarations")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (EMSCRIPTEN)
    message(STATUS "Building for WebAssembly via Emscripten")
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
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
    string(REPLACE "-pthread" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set_target_properties(wqxsim PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS}")
    
    # 创建一个包装脚本来生成ES6模块
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/es6_wrapper.js.in
        ${CMAKE_CURRENT_BINARY_DIR}/wqxsim.js
        @ONLY
    )
    
    set(EM_LINK_FLAGS
        "-sUSE_SDL=2"
        "-s MODULARIZE=1"
        "-s EXPORT_ES6=1"
        "-s EXPORT_NAME=\"WqxsimModule\""
        "-sEXPORTED_RUNTIME_METHODS=['FS','cwrap','ccall']"
        "-sINITIAL_MEMORY=128MB"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sASYNCIFY"
        "-sUSE_PTHREADS=0"
        "-sENVIRONMENT=web"
        "-DHAVE_HAPTIC=0"
        "-s WASM=1"
        "--bind"
        "-o ${CMAKE_CURRENT_BINARY_DIR}/wqxsim_internal.js"
    )
    
    # 先生成内部模块
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wqxsim_internal.js
        COMMAND em++ ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ansi/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/dsp/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/compare/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/lcdstripe/*.cpp -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CXX_FLAGS} ${EM_LINK_FLAGS}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ansi/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/dsp/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/compare/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/lcdstripe/*.cpp
        COMMENT "Building WASM module with ES6 export"
    )
    
    # 添加自定义目标来生成最终的ES6模块
    add_custom_target(wqxsim ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/wqxsim.js
        COMMENT "Creating ES6 module wrapper"
    )
    
    # 创建最终的ES6模块
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wqxsim.js
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/wqxsim_internal.js ${CMAKE_CURRENT_BINARY_DIR}/wqxsim.js
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/wqxsim_internal.js
        COMMENT "Copying ES6 module to final location"
    )
    set_target_properties(wqxsim PROPERTIES SUFFIX ".js")
    set_target_properties(wqxsim PROPERTIES LINK_FLAGS "${EM_LINK_FLAGS}")
else()
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
    target_link_libraries(wqxsim ${SDL2_LIBRARIES})
    if(WIN32)
        target_link_libraries(wqxsim wsock32 ws2_32)
    endif()
endif()
