cmake_minimum_required(VERSION 3.9.4)

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

#set(SHARED_LIBS_DIR "${CMAKE_SOURCE_DIR}/cnn-shared")
#set(SHARED_LIBS_DIR "/home/b/kropki/cnn/shared")

unset(USE_CNN)
#option(USE_CNN "Use CNN for kropla" ON)
set(USE_CNN ON)
message("Using CNN: " ${USE_CNN})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system iostreams)
find_package(ZLIB REQUIRED)

#list(APPEND CMAKE_PREFIX_PATH "/home/b/kropki/libtorch/libtorch")
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})


if(USE_CNN)
  add_subdirectory(src/torch)
  add_subdirectory(src/caffe)
endif()   # USE_CNN


add_subdirectory(external)

if(USE_CNN)
  message("Using CNN")
  set(CNN_src "src/get_cnn_prob.cc" "src/get_cnn_prob.h" "src/cnn_workers.cc" "src/cnn_workers.h" "src/cnn_hash_table.cc" "src/cnn_hash_table.h")
  set(CNN_lib "mcaffe" "mtorch")  # "${TORCH_LIBRARIES}") 
  #"libcaffe"  "mklml_intel" "iomp5" "mkldnn" "${Boost_LIBRARIES}" "${Boost_SYSTEM_LIBRARY}" "${GLOG_LIBRARY}" "stdc++fs" "mtorch" "${TORCH_LIBRARIES}") 

  add_executable(check_accuracy src/game.cc src/game.h src/history.cc src/history.h src/game_utils.h src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/montecarlo.cc src/montecarlo.h src/check_accuracy.cc src/safety.cc src/safety.h ${CNN_src})  # src/caffe/mcaffe.h src/caffe/mcaffe.cc
  target_include_directories(check_accuracy PRIVATE src)
  target_include_directories(check_accuracy PRIVATE  "${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")
  target_link_libraries(check_accuracy Threads::Threads mcaffe mtorch)
  #libcaffe  mklml_intel iomp5 mkldnn ${Boost_LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${GLOG_LIBRARY} stdc++fs)
else()
  message("Not using CNN")
  set(CNN_src "src/get_cnn_prob_dummy.cc" "src/get_cnn_prob.h")
  set(CNN_lib "")
endif()


  
add_executable(kropla src/game.cc src/game.h  src/history.cc src/history.h src/game_utils.h src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/montecarlo.cc src/montecarlo.h src/kropla_main.cc src/safety.cc src/safety.h ${CNN_src})
target_link_libraries(kropla Threads::Threads ${CNN_lib})
target_include_directories(kropla PRIVATE src)
#if (USE_CNN)
 #   target_include_directories(check_accuracy PRIVATE  "${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")
#endif()

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET kropla PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

message(STATUS "src: ${CNN_src}")
message(STATUS "--lib: ${CNN_lib}")

add_executable(gather src/game.cc src/game.h  src/history.cc src/history.h src/game_utils.h src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/generatedata.cc src/allpattgen.cc src/allpattgen.h  src/safety.cc src/safety.h src/montecarlo.cc src/montecarlo.h  ${CNN_src})
target_link_libraries(gather Threads::Threads ${CNN_lib})
target_include_directories(gather PRIVATE src)
#if (USE_CNN)
#    target_include_directories(gather PRIVATE  "${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")
#endif()

add_executable(extract src/game.cc src/game.h src/history.cc src/history.h src/game_utils.h src/string_utils.h src/string_utils.cc src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/montecarlo.cc src/montecarlo.h src/extract.cc src/safety.cc src/safety.h  src/gzip.cpp src/gzip.hpp ${CNN_src})
target_link_libraries(extract Threads::Threads HighFive ${CNN_lib} ${ZLIB_LIBRARIES}  ${Boost_LIBRARIES})
target_include_directories(extract PRIVATE src)
#if (USE_CNN)
#    target_include_directories(extract PRIVATE  "${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")
#endif()

if (USE_CNN)
  find_package(Torch REQUIRED)
  add_executable(extracttensors src/game.cc src/game.h src/history.cc src/history.h src/game_utils.h src/string_utils.h src/string_utils.cc src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/montecarlo.cc src/montecarlo.h src/extracttensors.cc src/safety.cc src/safety.h  src/gzip.cpp src/gzip.hpp
"src/get_cnn_prob_dummy.cc" "src/get_cnn_prob.h")
  target_link_libraries(extracttensors Threads::Threads ${CNN_lib} ${ZLIB_LIBRARIES}  ${Boost_LIBRARIES} "${TORCH_LIBRARIES}")
  target_include_directories(extracttensors PRIVATE src)
endif()

#if (USE_CNN)
#    target_include_directories(extracttensors PRIVATE  "${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")
#endif()



add_subdirectory(gtest)
enable_testing()

if(USE_CNN)
    set(MCAFFE_src "src/mcnn.h" "src/caffe/mcaffe.h" "src/caffe/mcaffe.cc") # "src/caffe/mtorch.cc")
#    list(APPEND CMAKE_PREFIX_PATH "/home/b/kropki/libtorch/libtorch")
#    find_package(Torch REQUIRED)
#    add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
else()
    set(MCAFFE_src "")
endif()

add_executable(runUnitTests
  unittest/board-test.cc
  unittest/game-test.cc
  unittest/history-test.cc
  unittest/utils-test.cc
  unittest/string-utils-test.cc
  unittest/safety-test.cc
  unittest/patt-test.cc
  unittest/extractutils-test.cc
 unittest/utils.cc
 unittest/utils.h
 src/group_neighbours.h
 src/group_neighbours.cc
 src/game.cc
 src/game.h
 src/gzip.cpp
 src/history.cc
 src/history.h 
 src/game_utils.h
 src/string_utils.h 
 src/string_utils.cc
 src/board.h
 src/board.cc
 src/sgf.cc
 src/board.h
 src/sgf.h
 src/command.cc
 src/patterns.cc
 src/allpattgen.h
 src/allpattgen.cc
 src/command.h
 src/patterns.h
 src/enclosure.cc
 src/enclosure.h
 src/threats.cc
 src/threats.h
 src/safety.cc
 src/safety.h
 src/montecarlo.cc
 src/montecarlo.h
# ${MCAFFE_src}
 ${CNN_src}
 )

target_compile_options(runUnitTests PUBLIC -g -O0)


target_link_libraries(runUnitTests gtest gmock gtest_main Threads::Threads ${CNN_lib}  ${ZLIB_LIBRARIES}  ${Boost_LIBRARIES})
target_include_directories(runUnitTests  PRIVATE ${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
target_include_directories(runUnitTests PRIVATE src)
#if (USE_CNN)
#    target_include_directories(runUnitTests PRIVATE  "${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")
#endif()
add_test(runUnitTests runUnitTests )
