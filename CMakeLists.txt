# CMakeLists.txt
cmake_minimum_required(VERSION 3.20) # cmake的最低版本要求

project(boost_asio CXX) # 项目名称 CXX表示C++项目

set(CMAKE_CXX_STANDARD_REQUIRED TRUE) # C++标准要求
set(CMAKE_CXX_STANDARD 20) # 设置C++标准版本

set(exeFileName main) # 设置可执行文件名称
add_executable(${exeFileName} main.cpp) # 添加可执行文件

# 设置CMake政策以避免警告
cmake_policy(SET CMP0167 NEW)

# 查找Boost库
# REQUIRED表示如果找不到Boost库，CMake会报错
# COMPONENTS用于指定需要的Boost组件
find_package(Boost REQUIRED COMPONENTS filesystem system locale)

# 如果找到了Boost库，Boost_INCLUDE_DIRS会包含头文件路径
# Boost_LIBRARIES会包含需要链接的库文件
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
endif()

# 链接Boost库到目标
target_link_libraries(${exeFileName} ${Boost_LIBRARIES})

# 编译选项
target_compile_options(${exeFileName} PUBLIC

    "-Wconversion" # 警告隐式转换
    "-Wunused-variable" # 警告未使用变量
    "-Wuninitialized" # 警告未初始化变量
    "-Weffc++" # 警告高效C++(Effective C++)
    "-Wno-return-type" # 警告未返回值
    "-Wdelete-non-virtual-dtor" # 警告delete时非虚析构函数
    "-Wparentheses" # 警告括号
    "-Wempty-body" # 警告空语句
    "-Wsuggest-override" # 警告建议override
    "-Werror=mismatched-new-delete" # new和delete不匹配
    "-Werror=return-type" # 返回值类型错误
    "-Wimplicit-fallthrough" # 隐式跌落(用于switch中不加break的情况)
)

add_subdirectory(src/01_base)
add_subdirectory(src/02_async)
add_subdirectory(src/03_beast-http)
add_subdirectory(src/04_beast-websocket)
add_subdirectory(src/05_ftp)