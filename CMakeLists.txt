####################################
# General project definition
####################################

CMAKE_MINIMUM_REQUIRED(VERSION 3.5.2 FATAL_ERROR)
SET(BUILD_SHARED_LIBS ON)

# Set cmake policy by version: https://cmake.org/cmake/help/latest/manual/cmake-policies.7.html
if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
  cmake_policy(VERSION 3.12)
endif()

# Define project
if(${CMAKE_VERSION} VERSION_GREATER 3.8.1)
  project(LLA
    VERSION 0.0.2
    DESCRIPTION "O2 LLA library"
    LANGUAGES CXX
  )
else()
  project(LLA
    VERSION 0.0.2
    LANGUAGES CXX
  )
endif()

# Documentation dir
add_subdirectory(doc)

# Add compiler flags for warnings and debug symbols
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")

# Set fPIC for all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set CMAKE_INSTALL_LIBDIR explicitly to lib (to avoid lib64 on CC7)
set(CMAKE_INSTALL_LIBDIR lib)

# Used only for debugging
set(CMAKE_BUILD_TYPE "Debug")

# Set the default build type to "RelWithDebInfo"
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo"
    CACHE
    STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel Coverage."
    FORCE
  )
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)

####################################
# Dependencies
####################################

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(${CMAKE_VERSION} VERSION_LESS 3.10)
  # needed for PythonInterp
  set(PYTHON_ROOT "/cvmfs/sft.cern.ch/lcg/releases/Python/2.7.16-e553a/x86_64-centos7-gcc8-opt/")
  set(CMAKE_PROGRAM_PATH "${PYTHON_ROOT}/bin")
  # needed for PythonLibs
  file(GLOB PYTHON_INCLUDE_DIR ${PYTHON_ROOT}/include/python*) 
  set(PYTHON_INCLUDE_DIR ${PYTHON_INCLUDE_DIR} CACHE PATH "Python include directory" FORCE)

  file(GLOB PYTHON_LIBRARY ${PYTHON_ROOT}/lib/libpython?.?.so)
  set(PYTHON_LIBRARY ${PYTHON_LIBRARY} CACHE PATH "Python library" FORCE)
  set(boost_python_component "python27")
  find_package(PythonLibs)
else()
  find_package(Python3 3.6 COMPONENTS Interpreter Development)
  if (Python3_FOUND)
    set(boost_python_component "python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}")
  else()
    message(WARNING "Python 3 was not found: falling back to Python 2")
    find_package(Python2 2.7 COMPONENTS Development REQUIRED)
    set(boost_python_component "python27")
  endif()
endif()

set(felix_deps FlxCard drivers_rcc regmap)
if(EXISTS $ENV{FELIX_ROOT})
  message("-- Detecting FELIX installation at $ENV{FELIX_ROOT}.")
  foreach(felix_dep ${felix_deps})
    string(TOLOWER ${felix_dep} dep_loc)
      include_directories($ENV{FELIX_ROOT}/${dep_loc})
      link_directories($ENV{FELIX_ROOT}/$ENV{FELIX_ARCH}/${dep_loc})
    endforeach(felix_dep)
else()
  message(FATAL_ERROR "-- Cannot find FELIX.")
endif()

find_package(FELIXwrapper REQUIRED)
find_package(InfoLogger REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost 1.5.6
  COMPONENTS
  unit_test_framework
  program_options
  filesystem
  ${boost_python_component}
  REQUIRED
)

####################################
# Module, library and executable definition
####################################

add_library(LLA SHARED
  src/InterprocessLockFactory.cxx
  src/Session.cxx
  src/SessionParameters.cxx
  src/commonException.cxx
  src/commonProgram.cxx
  src/commonSystem.cxx
)

target_sources(LLA PRIVATE
  $<$<BOOL:${Python2_FOUND} OR ${Python3_FOUND}>:src/PythonInterface.cxx>
  src/InterprocessLockBase.cxx
  src/NamedMutex.cxx
  src/LockParameters.cxx
  src/SocketLock.cxx
)

target_include_directories(LLA
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PYTHON_INCLUDE_DIR}
)

set(LINK_LIBS
  AliceO2::InfoLogger
  AliceO2::FELIXwrapper
  Boost::boost
  Boost::program_options
  Boost::filesystem
  Threads::Threads
)

# Link targets
target_link_libraries(LLA
  PUBLIC
    ${LINK_LIBS}
    ${Boost_LIBRARIES} ${PYTHON_LIBRARIES}
  PRIVATE
    $<$<BOOL:${Python2_FOUND}>:Boost::python27>
    $<$<BOOL:${Python2_FOUND}>:Python2::Python>
    $<$<BOOL:${Python3_FOUND}>:Boost::python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}>
    $<$<BOOL:${Python3_FOUND}>:Python3::Python>
)

set_target_properties(LLA
  PROPERTIES
  OUTPUT_NAME
    O2Lla # Adhere to O2 naming conventions
)

# Used for debugging purposes to expose more options for benchmarking
if((CMAKE_BUILD_TYPE STREQUAL "Debug"))
  if(${CMAKE_VERSION} VERSION_GREATER 3.9)
    add_compile_definitions(
      O2_LLA_BENCH_ENABLED
    )
  else()
    add_definitions(
      -DO2_LLA_BENCH_ENABLED
    )
  endif()
endif()


# Use C++17
target_compile_features(LLA PUBLIC cxx_std_17)

####################################
# Tests
####################################

enable_testing()
set(TEST_SRCS
  test/TestLock.cxx
  test/TestSession.cxx
)

foreach (test ${TEST_SRCS})
  include_directories(src)
  get_filename_component(test_name ${test} NAME)
  string(REGEX REPLACE ".cxx" "" test_name ${test_name})

  add_executable(${test_name} ${test})
  target_link_libraries(${test_name}
    PRIVATE
      LLA
      Boost::unit_test_framework
      Threads::Threads
  )
  add_test(NAME ${test_name} COMMAND ${test_name})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 15) # TODO: WHAT IS THIS?
endforeach()

####################################
# Executables
####################################

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(EXE_SRCS
    LlaBench.cxx
    ../src/example.cxx
    )

  set(EXE_NAMES
    o2-lla-bench
    lla-example
  )

  list(LENGTH EXE_SRCS count)
  math(EXPR count "${count}-1")
  foreach(i RANGE ${count})
    list(GET EXE_SRCS ${i} src)
    list(GET EXE_NAMES ${i} name)
    add_executable(${name} apps/${src})
    target_include_directories(${name}
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(${name}
      PRIVATE
        LLA
    )
  endforeach()

endif()

####################################
# Install
####################################

include(GNUInstallDirs)

# Build targets with install rpath on Mac to dramatically speed up installation
# https://gitlab.kitware.com/cmake/community/wikis/doc/cmake/RPATH-handling
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" isSystemDir)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
  endif()
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()
unset(isSystemDir)

# Install library and executables
install(TARGETS LLA ${EXE_NAMES}
  EXPORT LLATargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Create version file
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/cmake/LLAConfigVersion.cmake"
  VERSION ${PACKAGE_VERSION}
  COMPATIBILITY AnyNewerVersion
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/Lla DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Export targets
install(EXPORT LLATargets
  FILE
    LLATargets.cmake
  NAMESPACE
    o2::
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/LLA
)

# Configure and install Config files
configure_package_config_file(
  cmake/LLAConfig.cmake.in cmake/LLAConfig.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LLA"
  PATH_VARS CMAKE_INSTALL_PREFIX
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/LLAConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/LLAConfigVersion.cmake"
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/LLA
)
