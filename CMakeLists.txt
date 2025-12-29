cmake_minimum_required(VERSION 3.23)

project(kropla)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cppcheck.cmake)
include(clang-format.cmake)
include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

add_compile_options(-g -Wall -Wextra -Wpedantic -O3 -msse3 -march=native)
#set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Wpedantic -O3 -msse3 -march=native")
add_definitions(-DNDEBUG)  # -DSPEED_TEST -DDEBUG_SGF

#set(CMAKE_CXX_FLAGS "-s -static-libstdc++ -static-libgcc -Wall -std=c++17 -O3 -DNDEBUG -march=sandybridge")

unset(USE_CNN)
#option(USE_CNN "Use CNN for kropla" ON)
set(USE_CNN ON)
message("Using CNN: " ${USE_CNN})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system iostreams)
find_package(ZLIB REQUIRED)
find_package(GTest REQUIRED)

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})


if(USE_CNN)
  add_subdirectory(src/torch)
endif()   # USE_CNN

set(ENV{HIGHFIVE_UNIT_TESTS} OFF)
add_subdirectory(external)

if(USE_CNN)
  message("Using CNN")
  set(CNN_src "src/get_cnn_prob.cc" "src/get_cnn_prob.h" "src/cnn_workers.cc" "src/cnn_workers.h" "src/cnn_hash_table.cc" "src/cnn_hash_table.h")
  set(CNN_lib "mtorch")  # "${TORCH_LIBRARIES}") 
  #"libcaffe"  "mklml_intel" "iomp5" "mkldnn" "${Boost_LIBRARIES}" "${Boost_SYSTEM_LIBRARY}" "${GLOG_LIBRARY}" "stdc++fs" "mtorch" "${TORCH_LIBRARIES}") 
else()
  message("Not using CNN")
  set(CNN_src "src/get_cnn_prob_dummy.cc" "src/get_cnn_prob.h")
  set(CNN_lib "")
endif()



add_library(kroplalib STATIC
   src/game.cc
   src/game.h
   src/history.cc
   src/history.h
   src/game_utils.h
   src/group_neighbours.h
   src/group_neighbours.cc
   src/board.h
   src/board.cc
   src/sgf.cc
   src/sgf.h
   src/command.cc
   src/command.h
   src/patterns.cc
   src/patterns.h
   src/enclosure.cc
   src/enclosure.h
   src/threats.cc
   src/threats.h
   src/montecarlo.cc
   src/montecarlo.h
   src/simplegame.cc
   src/dfs.cc
   src/safety.cc
   src/safety.h
)


if(USE_CNN)
  add_executable(check_accuracy src/check_accuracy.cc ${CNN_src})
  target_include_directories(check_accuracy PRIVATE src)
  target_link_libraries(check_accuracy kroplalib Threads::Threads  mtorch)
endif()
  
add_executable(kropla src/kropla_main.cc  ${CNN_src})
target_link_libraries(kropla kroplalib Threads::Threads ${CNN_lib})
target_include_directories(kropla PRIVATE src)

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET kropla PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET kroplalib PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

message(STATUS "src: ${CNN_src}")
message(STATUS "--lib: ${CNN_lib}")

add_executable(gather src/generatedata.cc src/allpattgen.cc src/allpattgen.h  ${CNN_src})
target_link_libraries(gather kroplalib Threads::Threads ${CNN_lib})
target_include_directories(gather PRIVATE src)

add_executable(extract src/string_utils.h src/string_utils.cc src/extract.cc src/gzip.cpp src/gzip.hpp  ${CNN_src})
target_link_libraries(extract kroplalib Threads::Threads HighFive ${CNN_lib} ${ZLIB_LIBRARIES}  ${Boost_LIBRARIES})
target_include_directories(extract PRIVATE src)

if (USE_CNN)
  find_package(Torch REQUIRED)
  add_executable(extracttensors
   src/game.cc
   src/game.h
   src/history.cc
   src/history.h
   src/game_utils.h
   src/group_neighbours.h
   src/group_neighbours.cc
   src/board.h
   src/board.cc
   src/sgf.cc
   src/sgf.h
   src/command.cc
   src/command.h
   src/patterns.cc
   src/patterns.h
   src/enclosure.cc
   src/enclosure.h
   src/threats.cc
   src/threats.h
   src/montecarlo.cc
   src/montecarlo.h
   src/simplegame.cc
   src/dfs.cc
   src/safety.cc
   src/safety.h
   src/string_utils.h src/string_utils.cc src/extracttensors.cc src/gzip.cpp src/gzip.hpp "src/get_cnn_prob_dummy.cc" "src/get_cnn_prob.h")
  target_link_libraries(extracttensors  Threads::Threads ${ZLIB_LIBRARIES}  ${Boost_LIBRARIES} "${TORCH_LIBRARIES}")
  target_include_directories(extracttensors PRIVATE src)
  set_property(TARGET extracttensors PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

#add_subdirectory(gtest)
enable_testing()

add_executable(runUnitTests
  unittest/board-test.cc
  unittest/game-test.cc
  unittest/dfs-test.cc
  unittest/history-test.cc
  unittest/utils-test.cc
  unittest/string-utils-test.cc
  unittest/safety-test.cc
  unittest/patt-test.cc
  unittest/extractutils-test.cc
 unittest/utils.cc
 unittest/utils.h
 src/gzip.cpp
 src/string_utils.h 
 src/string_utils.cc
 src/allpattgen.h
 src/allpattgen.cc
 ${CNN_src}
)

target_compile_options(runUnitTests PUBLIC -g -O0)


target_link_libraries(runUnitTests kroplalib GTest::gtest GTest::gmock GTest::gtest_main Threads::Threads ${CNN_lib}  ${ZLIB_LIBRARIES}  ${Boost_LIBRARIES})
#target_include_directories(runUnitTests  PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
target_include_directories(runUnitTests PRIVATE src)

add_test(runUnitTests runUnitTests )
