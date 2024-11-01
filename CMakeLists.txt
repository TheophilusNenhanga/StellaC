cmake_minimum_required(VERSION 3.28)
project(StellaC C)

set(CMAKE_C_STANDARD 11)


if (CMAKE_BUILD_TYPE)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3")
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Profile")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
    endif ()
endif ()

add_executable(StellaC
        main.c
        common.h
        chunk.h
        chunk.c
        memory.h
        memory.c
        debug.h
        debug.c
        value.c
        value.h
        vm.c
        vm.h
        compiler.c
        compiler.h
        scanner.c
        scanner.h
        object.c
        table.c
        table.h
        object.h
        natives/io.h
        natives/io.c
        natives/time.h
        natives/time.c
        natives/collections.h
        natives/collections.c
        main.c
        natives/objects.c
        natives/objects.h)
