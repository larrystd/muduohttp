cmake_minimum_required(VERSION 2.6)

project(inspect C CXX)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})

add_custom_command(OUTPUT rpbcpp
  COMMAND protoc
  ARGS --cpp_out ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/rpc.proto -I ${PROJECT_SOURCE_DIR}
  DEPENDS rpc.proto
  VERBATIM)

add_custom_target(rpb
  DEPENDS rpbcpp
  )

set_source_files_properties(rpc.pb.cc PROPERTIES COMPILE_FLAGS "-Wno-conversion")
include_directories(${PROJECT_BINARY_DIR})
find_package(Boost REQUIRED)
include_directories(/home/larry/myproject/myc++proj/muduostd)
include_directories(/home/larry/myproject/myc++proj/muduostd/http)
include_directories(/home/larry/myproject/myc++proj/muduostd/protorpc)
include_directories(/usr/local/include)

link_directories(/home/larry/myproject/myc++proj/muduostd/build/lib)
link_directories(/usr/local/lib)



add_library(muduo_protorpc_wire rpc.pb.cc RpcCodec.cc)

set_target_properties(muduo_protorpc_wire PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")

#add_library(muduo_protorpc_wire_cpp11 rpc.pb.cc RpcCodec.cc)
#set_target_properties(muduo_protorpc_wire_cpp11 PROPERTIES COMPILE_FLAGS "-std=c++0x -Wno-error=shadow")
add_subdirectory(muduo_protobuf_codec)

add_executable(protobuf_rpc_wire_test RpcCodec_test.cc)
target_link_libraries(protobuf_rpc_wire_test muduo_protorpc_wire muduo_protobuf_codec muduo_net muduo_base protobuf pthread )
set_target_properties(protobuf_rpc_wire_test PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")

add_library(muduo_protorpc RpcChannel.cc RpcServer.cc)
set_target_properties(muduo_protorpc PROPERTIES COMPILE_FLAGS "-Wno-error=shadow")
target_link_libraries(muduo_protorpc muduo_protorpc_wire muduo_protobuf_codec muduo_net muduo_base protobuf z)

if(TCMALLOC_LIBRARY)
  target_link_libraries(muduo_protorpc tcmalloc_and_profiler)
endif()

install(TARGETS muduo_protorpc_wire muduo_protorpc DESTINATION lib)
#install(TARGETS muduo_protorpc_wire_cpp11 DESTINATION lib)

set(HEADERS
  RpcCodec.h
  RpcChannel.h
  RpcServer.h
  rpc.proto
  rpcservice.proto
  rpc.pb.h
  )
install(FILES ${HEADERS} DESTINATION include)

