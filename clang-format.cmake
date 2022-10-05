file(GLOB ALL_SOURCE_FILES
  ${PROJECT_SOURCE_DIR}/src/*.cc
  ${PROJECT_SOURCE_DIR}/src/*.h
  ${PROJECT_SOURCE_DIR}/src/caffe/*.cc
  ${PROJECT_SOURCE_DIR}/src/caffe/*.h
  ${PROJECT_SOURCE_DIR}/unittest/*.cc
  ${PROJECT_SOURCE_DIR}/unittest/*.h)

add_custom_target(
        clangformat
        COMMAND /usr/bin/clang-format-11
	-i
        ${ALL_SOURCE_FILES}
)
