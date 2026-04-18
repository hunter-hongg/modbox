# CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置 vcpkg 工具链文件（必须在 project() 之前）
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# 项目配置：名称modbox，语言C
project(modbox C)

# 设置编译输出目录（根目录/target）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target)

# 递归查找src/目录下所有.c源文件
file(GLOB_RECURSE SRC_FILES
    src/*.c
)

# 头文件搜索目录：include
include_directories(include)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# 查找 Argtable3 依赖（通过 vcpkg 安装）
find_package(Argtable3 CONFIG REQUIRED)

# 添加头文件路径
include_directories(${GLIB_INCLUDE_DIRS})

# 生成可执行文件modbox
add_executable(${PROJECT_NAME} ${SRC_FILES})

# 链接库
target_link_libraries(${PROJECT_NAME} 
    ${GLIB_LIBRARIES}
    argtable3::argtable3
)