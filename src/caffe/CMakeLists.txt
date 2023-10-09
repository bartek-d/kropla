cmake_minimum_required(VERSION 3.1)

  set(USE_OPENCV 0)
  set(USE_MLSL 0)

  find_package(Glog REQUIRED)
  #INCLUDE_DIRECTORIES(${GLOG_INCLUDE_DIR})

  add_library(libcaffe SHARED IMPORTED)
  set_target_properties(libcaffe PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/bin/lib/libcaffe.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/caffeintel/include" # "${CMAKE_SOURCE_DIR}/caffeintel/bin/include" 
    )

  target_include_directories(libcaffe INTERFACE "${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include" "${GLOG_INCLUDE_DIR}")

  #add_library(mlsl SHARED IMPORTED)
  #set_target_properties(mlsl PROPERTIES
  #  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/external/mlsl/l_mlsl_2018.1.005/intel64/lib/libmlsl.so"
  #)

  add_library(iomp5 SHARED IMPORTED)
  set_target_properties(iomp5 PROPERTIES
    #IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/external/mkl/mklml_lnx_2019.0.3.20190220/lib/libiomp5.so"
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/cnn-shared/libiomp5.so"    
    #IMPORTED_LOCATION "${SHARED_LIBS_DIR}/libiomp5.so"
    )

  add_library(mklml_intel SHARED IMPORTED)
  set_target_properties(mklml_intel PROPERTIES
      #IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/external/mkl/mklml_lnx_2019.0.3.20190220/lib/libmklml_intel.so"
     IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/cnn-shared/libmklml_intel.so"    
    #IMPORTED_LOCATION "${SHARED_LIBS_DIR}/libmklml_intel.so"
    )

  add_library(mkldnn SHARED IMPORTED)
  set_target_properties(mkldnn PROPERTIES
    #IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/caffeintel/external/mkldnn/install/lib/libmkldnn.so"
     IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/cnn-shared/libmkldnn.so"
    #IMPORTED_LOCATION "${SHARED_LIBS_DIR}/libmkldnn.so"
    )
  #set(MKLDNNROOT "${CMAKE_SOURCE_DIR}/external/mkldnn")

  #cmake_policy(SET CMP0079 NEW)
#  include_directories("${CMAKE_SOURCE_DIR}/caffeintel/include" "${CMAKE_SOURCE_DIR}/caffeintel/bin/include")

add_library(mcaffe SHARED mcaffe.cc mcaffe.h)
link_directories("${CMAKE_SOURCE_DIR}/caffeintel/external/mlsl/l_mlsl_2018.1.005/intel64/lib/")

target_link_libraries(mcaffe LINK_PRIVATE Threads::Threads  libcaffe mklml_intel iomp5 mkldnn ${Boost_LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${GLOG_LIBRARY}) # mlsl


