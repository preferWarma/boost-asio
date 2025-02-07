add_executable(Sever_01 server.cpp)
add_executable(Client_01 client.cpp)

target_include_directories(Sever_01 PUBLIC ../../include)
target_include_directories(Client_01 PUBLIC ../../include)

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
target_link_libraries(Sever_01 ${Boost_LIBRARIES})
target_link_libraries(Client_01 ${Boost_LIBRARIES})