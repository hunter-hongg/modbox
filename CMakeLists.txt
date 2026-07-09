cmake_minimum_required(VERSION 3.14)
project(modbox LANGUAGES CXX)

# Set toolchain file only if it exists
if(EXISTS "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target)

file(GLOB_RECURSE SRC_FILES
    src/*.cpp
)

include_directories(include)

find_package(PkgConfig REQUIRED)

# Ensure linuxbrew pkgconfig path is discoverable.
set(ENV{PKG_CONFIG_PATH} "/home/linuxbrew/.linuxbrew/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

# Argtable3
find_package(Argtable3 CONFIG QUIET)
if(NOT Argtable3_FOUND)
    pkg_check_modules(ARGTABLE3 REQUIRED argtable3)
endif()
if(ARGTABLE3_INCLUDE_DIRS)
    include_directories(${ARGTABLE3_INCLUDE_DIRS})
endif()
if(ARGTABLE3_LIBRARY_DIRS)
    link_directories(${ARGTABLE3_LIBRARY_DIRS})
endif()

# ftxui
pkg_check_modules(FTXUI REQUIRED ftxui)
if(FTXUI_INCLUDE_DIRS)
    include_directories(${FTXUI_INCLUDE_DIRS})
endif()
if(FTXUI_LIBRARY_DIRS)
    link_directories(${FTXUI_LIBRARY_DIRS})
endif()

# OpenSSL
pkg_check_modules(OPENSSL REQUIRED openssl)
if(OPENSSL_INCLUDE_DIRS)
    include_directories(${OPENSSL_INCLUDE_DIRS})
endif()
if(OPENSSL_LIBRARY_DIRS)
    link_directories(${OPENSSL_LIBRARY_DIRS})
endif()

add_executable(${PROJECT_NAME} ${SRC_FILES})

if(Argtable3_FOUND)
    target_link_libraries(${PROJECT_NAME} argtable3::argtable3)
else()
    target_link_libraries(${PROJECT_NAME} ${ARGTABLE3_LIBRARIES})
endif()

target_link_libraries(${PROJECT_NAME} ${FTXUI_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})

# C++20 std::filesystem needs linking on some platforms
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} stdc++fs)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CLANG_TIDY clang-tidy)

option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis during build" OFF)

if(ENABLE_CLANG_TIDY)
    if(CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}" --use-color --system-headers=false)
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy was not found")
    endif()
endif()

if(CLANG_TIDY)
    add_custom_target(lint
        COMMAND ${CLANG_TIDY}
                -p ${CMAKE_BINARY_DIR}
                --use-color
                --system-headers=false
                ${SRC_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Linting source files with clang-tidy"
    )
else()
    add_custom_target(lint
        COMMENT "Lint unavailable: install clang-tidy"
    )
endif()
