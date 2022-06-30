cmake_minimum_required(VERSION 3.1)

project(kropla)
set(CMAKE_CXX_FLAGS "-g -Wall -std=c++17 -O0 -DNDEBUG -msse3")
#add_compile_options(-s -Wall -std=c++17 -O3 -DNDEBUG -msse3)
#set(CMAKE_CXX_FLAGS "-s  -std=c++17 -O3 -DNDEBUG -msse3")
#set(CMAKE_CXX_FLAGS "-s -static-libstdc++ -static-libgcc  -Wall -std=c++17 -O3 -DNDEBUG -msse3 -DSPEED_TEST")
#set(CMAKE_CXX_FLAGS "-s -static-libstdc++ -static-libgcc  -Wall -std=c++17 -O3 -DNDEBUG -msse3")
#set(CMAKE_CXX_FLAGS "-s -static-libstdc++ -static-libgcc  -Wall -std=c++17 -O3 -DNDEBUG -DDEBUG_SGF -msse3")

set(SHARED_LIBS_DIR "/home/b/kropki/cnn/shared")
unset(USE_CNN)
option(USE_CNN "Use CNN for kropla" ON)
#set(USE_CNN ON)
message("Using CNN: " ${USE_CNN})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})

#find_package(OpenCV REQUIRED)
include_directories(src)

if(USE_CNN)
  set(USE_OPENCV 0)
  set(USE_MLSL 0)

  find_package(Glog REQUIRED)
  INCLUDE_DIRECTORIES(${GLOG_INCLUDE_DIR})

  add_subdirectory(caffeintel)
  include_directories("${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")

  add_library(libcaffe SHARED IMPORTED)
  set_target_properties(libcaffe PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/bin/lib/libcaffe.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/caffeintel/include" # "${CMAKE_SOURCE_DIR}/caffeintel/bin/include" 
    )

  #add_library(mlsl SHARED IMPORTED)
  #set_target_properties(mlsl PROPERTIES
  #  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/external/mlsl/l_mlsl_2018.1.005/intel64/lib/libmlsl.so"
  #)

  add_library(iomp5 SHARED IMPORTED)
  set_target_properties(iomp5 PROPERTIES
    #  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/external/mkl/mklml_lnx_2019.0.3.20190220/lib/libiomp5.so"
    IMPORTED_LOCATION "${SHARED_LIBS_DIR}/libiomp5.so"
    )

  add_library(mklml_intel SHARED IMPORTED)
  set_target_properties(mklml_intel PROPERTIES
    #  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/external/mkl/mklml_lnx_2019.0.3.20190220/lib/libmklml_intel.so"
    IMPORTED_LOCATION "${SHARED_LIBS_DIR}/libmklml_intel.so"
    )

  add_library(mkldnn SHARED IMPORTED)
  set_target_properties(mkldnn PROPERTIES
    #  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/external/mkldnn/install/lib/libmkldnn.so"
    IMPORTED_LOCATION "${SHARED_LIBS_DIR}/libmkldnn.so"
    )
  #set(MKLDNNROOT "${CMAKE_SOURCE_DIR}/external/mkldnn")

  #cmake_policy(SET CMP0079 NEW)
  include_directories("${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")
  add_subdirectory(src/caffe)

endif()   # USE_CNN


include_directories(src)
add_subdirectory(external)

if(USE_CNN)
  message("Using CNN")
  set(CNN_src "src/get_cnn_prob.cc" "src/get_cnn_prob.h")
  set(CNN_lib "mcaffe" "libcaffe"  "mklml_intel" "iomp5" "mkldnn" "${Boost_LIBRARIES}" "${Boost_SYSTEM_LIBRARY}" "${GLOG_LIBRARY}" "stdc++fs") 

  add_executable(check_accuracy src/game.cc src/game.h src/game_utils.h src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/montecarlo.cc src/montecarlo.h src/check_accuracy.cc src/safety.cc src/safety.h src/caffe/mcaffe.h src/caffe/mcaffe.cc ${CNN_src})
  target_link_libraries(check_accuracy Threads::Threads mcaffe libcaffe  mklml_intel iomp5 mkldnn ${Boost_LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${GLOG_LIBRARY} stdc++fs)
else()
  message("Not using CNN")
  set(CNN_src "src/get_cnn_prob_dummy.cc" "src/get_cnn_prob.h")
  set(CNN_lib "")
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_library(STD_CPP_FS stdc++fs)




  
add_executable(kropla src/game.cc src/game.h src/game_utils.h src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/montecarlo.cc src/montecarlo.h src/kropla_main.cc src/safety.cc src/safety.h ${CNN_src})
target_link_libraries(kropla Threads::Threads ${CNN_lib})

add_executable(gather src/game.cc src/game.h src/game_utils.h src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/generatedata.cc src/allpattgen.cc src/allpattgen.h  src/safety.cc src/safety.h src/montecarlo.cc src/montecarlo.h  ${CNN_src})
target_link_libraries(gather Threads::Threads ${CNN_lib})

add_executable(extract src/game.cc src/game.h src/game_utils.h src/group_neighbours.h src/group_neighbours.cc src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/montecarlo.cc src/montecarlo.h src/extract.cc src/safety.cc src/safety.h ${CNN_src})
target_link_libraries(extract Threads::Threads HighFive ${CNN_lib})

if(NOT USE_CNN)
add_subdirectory(gtest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
#include_directories("${CMAKE_SOURCE_DIR}/caffeintel/src/gmock/include" "${CMAKE_SOURCE_DIR}/caffeintel/src/gtest/include")

add_executable(runUnitTests
  unittest/board-test.cc
  unittest/game-test.cc
  unittest/utils-test.cc
  unittest/safety-test.cc
  unittest/patt-test.cc
 unittest/utils.cc
 unittest/utils.h
 src/group_neighbours.h
 src/group_neighbours.cc
 src/game.cc
 src/game.h
 src/game_utils.h
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
 ${CNN_src}
 )
  

target_link_libraries(runUnitTests gtest gmock gtest_main)
add_test(runUnitTests runUnitTests )
endif()

#add_executable(mojecnn src/mcaffe.cpp   src/czytanie.cpp  src/mcaffe.hpp src/util.h src/util.c)

#link_directories("${CMAKE_SOURCE_DIR}/caffeintel/external/mlsl/l_mlsl_2018.1.005/intel64/lib/")

#target_link_libraries(mojecnn LINK_PUBLIC Threads::Threads  libcaffe mlsl mklml_intel iomp5 ${Boost_LIBRARIES} ${Boost_SYSTEM_LIBRARY} )
#target_link_libraries(mojecnn  Threads::Threads  libcaffe mklml_intel iomp5 mkldnn ${Boost_LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${GLOG_LIBRARY}) # mlsl
