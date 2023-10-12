cmake_minimum_required(VERSION 3.1)


set(CMAKE_CXX_STANDARD 20)

add_library(mtorch SHARED mtorch.cc mtorch.h)
list(APPEND CMAKE_PREFIX_PATH "/home/b/kropki/libtorch/libtorch")
find_package(Torch REQUIRED)
target_link_libraries(mtorch LINK_PRIVATE Threads::Threads "${TORCH_LIBRARIES}")

