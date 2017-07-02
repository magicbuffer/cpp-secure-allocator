cmake_minimum_required(VERSION 3.7)

project(cpp-secure-allocator)

add_executable(cpp-secure-allocator src/sample.cpp)

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

#target_compile_features(cpp-secure-allocator PRIVATE cxx_range_for)
target_include_directories(cpp-secure-allocator PUBLIC include)

#target_sources(cpp-secure-allocator
#    PRIVATE
#        "${CMAKE_CURRENT_SOURCE_DIR}/src/sample.cpp"
#)