add_executable(Sever_02 server.cpp)
add_executable(Client_02 client.cpp)

target_include_directories(Sever_02 PUBLIC ../../include)
target_include_directories(Client_02 PUBLIC ../../include)

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
target_link_libraries(Sever_02 ${Boost_LIBRARIES})
target_link_libraries(Client_02 ${Boost_LIBRARIES})