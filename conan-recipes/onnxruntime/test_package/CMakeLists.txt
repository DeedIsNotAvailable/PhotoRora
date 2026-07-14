cmake_minimum_required(VERSION 3.8)

project(example CXX)

find_package(onnxruntime REQUIRED CONFIG)

add_executable(example src/example.cpp)
if(WITH_CUDA)
  target_compile_definitions(example PRIVATE WITH_CUDA)
endif()
target_link_libraries(example PRIVATE onnxruntime::onnxruntime)
target_compile_features(example PRIVATE cxx_std_17)
