cmake_minimum_required(VERSION 3.1)
project(dsip)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14 -stdlib=libc++ -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14 -stdlib=libc++ -O3")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")

add_subdirectory(deps/glfw)

include_directories(deps/glfw/deps) # for glad
include_directories(deps/glfw/include)
include_directories(deps)

add_library(flag deps/flag.c)

set(SOURCES
  src/Window.cpp
  src/Shader.cpp
  src/Util.mm
  src/Image.cpp)

macro(add_bundle_resources RESOURCE_LIST RESOURCE_DIR RESOURCE_BASE)
  file(GLOB_RECURSE FULL_RESOURCE_PATHS "${RESOURCE_DIR}/[^.]**")
  foreach(_full_resource_path ${FULL_RESOURCE_PATHS})
    file(RELATIVE_PATH REL_RESOURCE_PATH ${RESOURCE_DIR} ${_full_resource_path})
    get_filename_component(REL_RESOURCE_DIR ${REL_RESOURCE_PATH} PATH)
    set_source_files_properties(${_full_resource_path} PROPERTIES MACOSX_PACKAGE_LOCATION "${RESOURCE_BASE}/${REL_RESOURCE_DIR}")
  endforeach()
  list(APPEND ${RESOURCE_LIST} ${FULL_RESOURCE_PATHS})
endmacro(add_bundle_resources)

add_bundle_resources(RESOURCES "${PROJECT_SOURCE_DIR}/data" "Resources")

add_executable(dsip MACOSX_BUNDLE ${SOURCES} ${RESOURCES} src/main.cpp deps/glfw/deps/glad.c)

target_compile_definitions(dsip PRIVATE DSIP_GUI=1)

set_target_properties(dsip PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/Info.plist")

target_link_libraries(dsip flag glfw ${GLFW_LIBRARIES})

set(SOURCES_CLI
  src/Image.cpp
  src/Util.mm)

add_executable(dsip-cli ${SOURCES_CLI} src/main.cpp)

target_link_libraries(dsip-cli flag)
