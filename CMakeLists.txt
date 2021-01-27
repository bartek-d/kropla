cmake_minimum_required(VERSION 3.1)

project(kropla)
set(CMAKE_CXX_FLAGS "-s -Wall -std=c++17 -O3 -DNDEBUG -msse3")
#set(CMAKE_CXX_FLAGS "-s -static-libstdc++ -static-libgcc  -Wall -std=c++17 -O3 -DNDEBUG -msse3 -DSPEED_TEST")
#set(CMAKE_CXX_FLAGS "-s -static-libstdc++ -static-libgcc  -Wall -std=c++17 -O3 -DNDEBUG -msse3")
#set(CMAKE_CXX_FLAGS "-s -static-libstdc++ -static-libgcc  -Wall -std=c++17 -O3 -DNDEBUG -DDEBUG_SGF -msse3")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
include_directories(src)

add_executable(kropla src/game.cc src/game.h src/game_utils.h src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/montecarlo.cc src/montecarlo.h src/kropla_main.cc)
target_link_libraries(kropla Threads::Threads)

add_executable(gather src/game.cc src/game.h src/game_utils.h src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h src/generatedata.cc)
target_link_libraries(gather Threads::Threads)

add_subdirectory(gtest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
add_executable(runUnitTests unittest/board-test.cc unittest/game-test.cc unittest/utils-test.cc unittest/utils.cc unittest/utils.h  src/game.cc src/game.h src/game_utils.h src/board.h src/board.cc  src/sgf.cc  src/board.h     src/sgf.h src/command.cc   src/patterns.cc  src/command.h   src/patterns.h src/enclosure.cc src/enclosure.h src/threats.cc src/threats.h)
target_link_libraries(runUnitTests gtest gtest_main)
add_test(runUnitTests runUnitTests )


