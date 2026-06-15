cmake_minimum_required(VERSION 3.24)

project(paic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(ProcessorCount)

# ------------------------------------------------------------
# User options
# ------------------------------------------------------------

option(PAIC_BUILD_R_MODULES "Build R .Call shared-object modules" ON)
option(PAIC_BUILD_PYTHON_CTYPES "Build Python ctypes shared libraries" OFF)
option(PAIC_USE_STAN_THREADS "Compile Stan Math with STAN_THREADS" ON)
option(PAIC_USE_OPENMP "Link OpenMP to PAIC modules" OFF)

set(PAIC_MODULES
  logistic
  CACHE STRING "List of PAIC model subdirectories"
)

# ------------------------------------------------------------
# Dependencies: Eigen, Stan Math
# ------------------------------------------------------------

FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG        3.4.0
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  stan_math
  GIT_REPOSITORY https://github.com/stan-dev/math.git
  GIT_TAG        v5.3.0
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(eigen)

FetchContent_GetProperties(stan_math)
if(NOT stan_math_POPULATED)
  FetchContent_MakeAvailable(stan_math)
endif()

find_package(Threads REQUIRED)

if(PAIC_USE_OPENMP)
  find_package(OpenMP REQUIRED)
endif()

# ------------------------------------------------------------
# Build Stan Math binary dependencies
# ------------------------------------------------------------

find_program(MAKE_EXECUTABLE NAMES make gmake REQUIRED)

ProcessorCount(NPROC)
if(NOT NPROC OR NPROC EQUAL 0)
  set(NPROC 2)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(STAN_TBB_CXX_TYPE "clang" CACHE STRING "Stan Math TBB compiler type")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(STAN_TBB_CXX_TYPE "gcc" CACHE STRING "Stan Math TBB compiler type")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
  set(STAN_TBB_CXX_TYPE "clang" CACHE STRING "Stan Math TBB compiler type")
else()
  set(STAN_TBB_CXX_TYPE "gcc" CACHE STRING "Stan Math TBB compiler type")
  message(WARNING
    "Unrecognized CMAKE_CXX_COMPILER_ID='${CMAKE_CXX_COMPILER_ID}'. "
    "Defaulting STAN_TBB_CXX_TYPE to gcc. Override with "
    "-DSTAN_TBB_CXX_TYPE=clang or -DSTAN_TBB_CXX_TYPE=gcc if needed."
  )
endif()

set_property(CACHE STAN_TBB_CXX_TYPE PROPERTY STRINGS gcc clang)

add_custom_target(
  stan_math_binary_deps
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "TBB_CXX_TYPE=${STAN_TBB_CXX_TYPE}"
    "${MAKE_EXECUTABLE}"
    "-j${NPROC}"
    "-f" "${stan_math_SOURCE_DIR}/make/standalone"
    "math-libs"
    "CXX=${CMAKE_CXX_COMPILER}"
    "TBB_CXX_TYPE=${STAN_TBB_CXX_TYPE}"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Building Stan Math binary dependencies"
  VERBATIM
)

function(first_matching_directory output_variable glob_pattern)
  file(GLOB matches LIST_DIRECTORIES true "${glob_pattern}")
  if(NOT matches)
    message(FATAL_ERROR "Could not find directory matching: ${glob_pattern}")
  endif()
  list(GET matches 0 first_match)
  set(${output_variable} "${first_match}" PARENT_SCOPE)
endfunction()

first_matching_directory(
  STAN_BOOST_DIR
  "${stan_math_SOURCE_DIR}/lib/boost_*"
)

first_matching_directory(
  STAN_SUNDIALS_DIR
  "${stan_math_SOURCE_DIR}/lib/sundials_*"
)

first_matching_directory(
  STAN_TBB_INCLUDE_DIR
  "${stan_math_SOURCE_DIR}/lib/tbb_*"
)

set(STAN_TBB_LIBRARY_DIR "${stan_math_SOURCE_DIR}/lib/tbb")

# ------------------------------------------------------------
# Stan Math interface target
# ------------------------------------------------------------

add_library(paic_stan_math INTERFACE)

target_compile_features(
  paic_stan_math
  INTERFACE
    cxx_std_17
)

target_compile_definitions(
  paic_stan_math
  INTERFACE
    BOOST_DISABLE_ASSERTS
    EIGEN_NO_DEBUG
    _REENTRANT
)

if(PAIC_USE_STAN_THREADS)
  target_compile_definitions(
    paic_stan_math
    INTERFACE
      STAN_THREADS
  )
endif()

target_include_directories(
  paic_stan_math
  INTERFACE
    "${stan_math_SOURCE_DIR}"
    "${STAN_BOOST_DIR}"
    "${STAN_SUNDIALS_DIR}/include"
    "${STAN_TBB_INCLUDE_DIR}/include"
)

target_link_directories(
  paic_stan_math
  INTERFACE
    "${STAN_TBB_LIBRARY_DIR}"
    "${STAN_SUNDIALS_DIR}/lib"
)

target_link_libraries(
  paic_stan_math
  INTERFACE
    Eigen3::Eigen
    Threads::Threads
    tbb
    sundials_cvodes
    sundials_idas
    sundials_kinsol
    sundials_nvecserial
)

if(UNIX AND NOT APPLE)
  target_link_libraries(
    paic_stan_math
    INTERFACE
      dl
  )
endif()

if(UNIX)
  target_link_options(
    paic_stan_math
    INTERFACE
      "-Wl"
  )
endif()

# ------------------------------------------------------------
# Root utility/header-only common target
# ------------------------------------------------------------

add_library(paic_common INTERFACE)

target_include_directories(
  paic_common
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
  paic_common
  INTERFACE
    paic_stan_math
    ${PAIC_SIMDJSON_TARGET}
)

if(PAIC_USE_OPENMP)
  target_link_libraries(
    paic_common
    INTERFACE
      OpenMP::OpenMP_CXX
  )
endif()

# ------------------------------------------------------------
# R support
# ------------------------------------------------------------

if(PAIC_BUILD_R_MODULES)
  find_program(RSCRIPT_EXECUTABLE NAMES Rscript REQUIRED)

  execute_process(
    COMMAND "${RSCRIPT_EXECUTABLE}" -e "cat(R.home())"
    OUTPUT_VARIABLE R_HOME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )

  execute_process(
    COMMAND "${RSCRIPT_EXECUTABLE}" -e "cat(R.home('include'))"
    OUTPUT_VARIABLE R_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )

  execute_process(
    COMMAND "${RSCRIPT_EXECUTABLE}" -e "cat(R.home('lib'))"
    OUTPUT_VARIABLE R_LIBRARY_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )

  message(STATUS "R_HOME: ${R_HOME}")
  message(STATUS "R include dir: ${R_INCLUDE_DIR}")
  message(STATUS "R library dir: ${R_LIBRARY_DIR}")

  if(NOT EXISTS "${R_INCLUDE_DIR}/R.h")
    message(FATAL_ERROR "Could not find R.h at ${R_INCLUDE_DIR}/R.h")
  endif()
endif()

# ------------------------------------------------------------
# Helper: R module target
# ------------------------------------------------------------

function(paic_add_r_module module_name)
  if(NOT PAIC_BUILD_R_MODULES)
    return()
  endif()

  add_library(${module_name}_r MODULE ${ARGN})

  add_dependencies(${module_name}_r stan_math_binary_deps)

  target_include_directories(
    ${module_name}_r
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${R_INCLUDE_DIR}"
  )

  target_link_directories(
    ${module_name}_r
    PRIVATE
      "${R_LIBRARY_DIR}"
  )

  target_link_libraries(
    ${module_name}_r
    PRIVATE
      paic_common
  )

  set_target_properties(
    ${module_name}_r
    PROPERTIES
      PREFIX ""
      OUTPUT_NAME "${module_name}_r"
      POSITION_INDEPENDENT_CODE ON
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )

  # R conventionally dyn.load()s .so files on macOS, not .dylib.
  if(APPLE)
    set_target_properties(${module_name}_r PROPERTIES SUFFIX ".so")
    target_link_options(
      ${module_name}_r
      PRIVATE
        "-undefined"
        "dynamic_lookup"
    )
  elseif(UNIX)
    target_link_libraries(
      ${module_name}_r
      PRIVATE
        R
    )
  endif()
endfunction()

# ------------------------------------------------------------
# Helper: Python ctypes shared library target
# ------------------------------------------------------------

function(paic_add_python_ctypes_module module_name)
  if(NOT PAIC_BUILD_PYTHON_CTYPES)
    return()
  endif()

  add_library(${module_name}_ctypes SHARED ${ARGN})

  add_dependencies(${module_name}_ctypes stan_math_binary_deps)

  target_include_directories(
    ${module_name}_ctypes
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}"
  )

  target_link_libraries(
    ${module_name}_ctypes
    PRIVATE
      paic_common
  )

  set_target_properties(
    ${module_name}_ctypes
    PROPERTIES
      OUTPUT_NAME "${module_name}_ctypes"
      POSITION_INDEPENDENT_CODE ON
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
endfunction()

# ------------------------------------------------------------
# Add model subdirectories
# ------------------------------------------------------------

foreach(module IN LISTS PAIC_MODULES)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module}/CMakeLists.txt")
    add_subdirectory("${module}")
  else()
    message(FATAL_ERROR
      "PAIC module '${module}' is listed but "
      "${CMAKE_CURRENT_SOURCE_DIR}/${module}/CMakeLists.txt does not exist"
    )
  endif()
endforeach()
