cmake_minimum_required(VERSION 3.1)

add_library(mcaffe STATIC mcaffe.cc mcaffe.h)
link_directories("${CMAKE_SOURCE_DIR}/caffeintel/external/mlsl/l_mlsl_2018.1.005/intel64/lib/")

target_link_libraries(mcaffe LINK_PRIVATE Threads::Threads  libcaffe mklml_intel iomp5 mkldnn ${Boost_LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${GLOG_LIBRARY}) # mlsl



