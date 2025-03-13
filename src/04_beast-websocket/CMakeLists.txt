add_executable(WebSocketServer RunServer.cpp Connection.cpp)

target_include_directories(WebSocketServer PUBLIC ../../include)

# 设置CMake政策以避免警告
cmake_policy(SET CMP0167 NEW)

# 查找Boost库
# REQUIRED表示如果找不到Boost库，CMake会报错
# COMPONENTS用于指定需要的Boost组件
find_package(Boost REQUIRED COMPONENTS filesystem system)

# 如果找到了Boost库，Boost_INCLUDE_DIRS会包含头文件路径
# Boost_LIBRARIES会包含需要链接的库文件
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
endif()

# 链接Boost库到目标
target_link_libraries(WebSocketServer ${Boost_LIBRARIES})

# 查找 jsoncpp
find_package(JsonCpp REQUIRED)

# 包含 jsoncpp 的头文件路径
include_directories(${JSONCPP_INCLUDE_DIRS})

# 链接 jsoncpp 库
target_link_libraries(WebSocketServer JsonCpp::JsonCpp)