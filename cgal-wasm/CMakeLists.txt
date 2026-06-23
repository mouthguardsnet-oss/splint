cmake_minimum_required(VERSION 3.20)
project(cgal_splint_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CGAL_ROOT)
  message(FATAL_ERROR "Set CGAL_ROOT to the unpacked CGAL source directory.")
endif()

if(NOT DEFINED BOOST_INCLUDE_DIR)
  message(FATAL_ERROR "Set BOOST_INCLUDE_DIR to the Boost headers directory.")
endif()

file(GLOB CGAL_PACKAGE_INCLUDE_DIRS CONFIGURE_DEPENDS "${CGAL_ROOT}/*/include")
list(FILTER CGAL_PACKAGE_INCLUDE_DIRS EXCLUDE REGEX "/CGAL_Core/include$")

if(NOT CGAL_PACKAGE_INCLUDE_DIRS)
  message(FATAL_ERROR "No CGAL package include directories found in CGAL_ROOT=${CGAL_ROOT}")
endif()

add_executable(cgal_splint src/cgal_splint.cpp)
target_include_directories(cgal_splint PRIVATE
  "${BOOST_INCLUDE_DIR}"
  ${CGAL_PACKAGE_INCLUDE_DIRS}
)

target_compile_definitions(cgal_splint PRIVATE
  CGAL_HEADER_ONLY=1
  CGAL_DISABLE_ROUNDING_MATH_CHECK=ON
  CGAL_NO_GMP=1
  CGAL_NO_MPFR=1
  CGAL_NO_CORE=1
)

if(EMSCRIPTEN)
  set_target_properties(cgal_splint PROPERTIES
    OUTPUT_NAME "cgal-splint"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
  )

  target_link_options(cgal_splint PRIVATE
    "SHELL:-O3"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_ES6=1"
    "SHELL:-s ENVIRONMENT=web,worker"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s MAXIMUM_MEMORY=2147483648"
    "SHELL:-s EXPORTED_FUNCTIONS=['_malloc','_free','_cgal_process','_cgal_generate_splint','_cgal_get_projected_ptr','_cgal_get_projected_count','_cgal_get_generated_positions_ptr','_cgal_get_generated_positions_count','_cgal_get_report_ptr','_cgal_get_border_halfedge_count','_cgal_get_invalid_face_count','_cgal_get_self_intersection_count']"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['HEAPF64','HEAPU32','UTF8ToString']"
  )
endif()
