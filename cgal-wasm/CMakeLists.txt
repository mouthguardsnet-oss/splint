cmake_minimum_required(VERSION 3.20)
project(cgal_splint_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CGAL REQUIRED)

add_executable(cgal_splint src/cgal_splint.cpp)
target_link_libraries(cgal_splint PRIVATE CGAL::CGAL)

target_compile_definitions(cgal_splint PRIVATE
  CGAL_DISABLE_ROUNDING_MATH_CHECK=ON
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
    "SHELL:-s EXPORTED_FUNCTIONS=['_malloc','_free','_cgal_process','_cgal_get_projected_ptr','_cgal_get_projected_count','_cgal_get_report_ptr','_cgal_get_border_halfedge_count','_cgal_get_invalid_face_count','_cgal_get_self_intersection_count']"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['HEAPF64','HEAPU32','UTF8ToString']"
  )
endif()
