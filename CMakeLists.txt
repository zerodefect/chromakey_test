
# CMake project file.

# At the time, Gavin was using version 3.7.
cmake_minimum_required(VERSION 3.4)

project(chroma_key_test)

set(CMAKE_CXX_STANDARD 17)

message(STATUS "Project:    ${PROJECT_NAME}")
message(STATUS "Platform:   ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM})")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")


#set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)


# Set the location where our binaries will be stored.
# WARN/TODO: Not quite right, because .lib or .a files should
#            not go in the bin folder!
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Adds 'd' onto artifacts - does NOT apply to executables.
# For executables, this needs to be done on an exec-by-exec
# basis.
set(CMAKE_DEBUG_POSTFIX "d")

# Local header files here ONLY
set(TARGET_HPP

    )

# Local source files here
set(TARGET_CPP
    main.cpp
    )

# Define an executable
add_executable(cb_chroma_key_test ${TARGET_HPP} ${TARGET_CPP})

# Additional Include Directories
target_include_directories(cb_chroma_key_test PRIVATE

    #Third Party SDKS

    #Local Directories

    )

# Define the libraries this project depends upon
target_link_libraries(cb_chroma_key_test

    # Local Libraries

    # TPSDKS
    pthread
    rt
    avcodec
    avformat
    avfilter
    avutil
    )
