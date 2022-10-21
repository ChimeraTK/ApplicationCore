PROJECT(ChimeraTK-ApplicationCore)
cmake_minimum_required(VERSION 2.8)

include(cmake/add_dependency.cmake)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 03)
set(${PROJECT_NAME}_MINOR_VERSION 00)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

option(BUILD_TESTS "Build tests." ON)

# Find the ControlSystemAdapter
add_dependency(ChimeraTK-ControlSystemAdapter 02.04 REQUIRED)

# Find the DeviceAccess
# Although all the linker flags are added again, the include path might be different
# if the libraries are installed in different directories for development purposes.
add_dependency(ChimeraTK-DeviceAccess 03.03 REQUIRED)

# Find the XML parser library libxml++
FIND_PACKAGE(PkgConfig REQUIRED)
set(LIBXML++_VERSION "libxml++-2.6")
PKG_CHECK_MODULES(LibXML++ REQUIRED ${LIBXML++_VERSION})
PKG_CHECK_MODULES(glib REQUIRED glib-2.0)
include_directories(SYSTEM ${glib_INCLUDE_DIRS} ${LibXML++_INCLUDE_DIRS})

# Find BOOST filesystem
# Note: we need to search this after looking for the unit_test_framework, since we must not link against the
#       unit_test_framework library (use header-only)
FIND_PACKAGE(Boost COMPONENTS filesystem date_time chrono system thread REQUIRED)

include(cmake/set_default_build_to_release.cmake)
include(cmake/set_default_flags.cmake)
include(cmake/enable_code_coverage_report.cmake)

# linker flags
if(NOT APPLE)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic")
endif()
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ChimeraTK-ControlSystemAdapter_LINK_FLAGS}")

include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB headers "${CMAKE_SOURCE_DIR}/include/*.h")
set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include/ ${CMAKE_SOURCE_DIR}/Modules/include/)
aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

# add generic modules
include_directories(${CMAKE_SOURCE_DIR}/Modules/include)
file(GLOB module_headers "${CMAKE_SOURCE_DIR}/Modules/include/*.h")
aux_source_directory(${CMAKE_SOURCE_DIR}/Modules/src library_sources)

MACRO( COPY_MAPPING_FILES )
  foreach( FILE_TO_COPY test.xlmap test.dmap )
    configure_file( tests/${FILE_TO_COPY} ${PROJECT_BINARY_DIR}/${FILE_TO_COPY} copyonly)
  endforeach( FILE_TO_COPY )
ENDMACRO( COPY_MAPPING_FILES )

# Create the executables for automated unit testing.
option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
endif()

# C++ library
add_library(${PROJECT_NAME} SHARED ${library_sources} ${headers} ${module_headers})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                 SOVERSION ${${PROJECT_NAME}_SOVERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "${ChimeraTK-ControlSystemAdapter_LINK_FLAGS}")
target_link_libraries(${PROJECT_NAME} ${ChimeraTK-ControlSystemAdapter_LIBRARIES}
                                      ${Boost_LIBRARIES}
                                      pthread
                                      ${LibXML++_LIBRARIES}
                                      ${glib_LIBRARIES}
                                      ${HDF5_LIBRARIES})

# do not remove runtime path of the library when installing
set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)

# add a target to generate API documentation with Doxygen
include(cmake/enable_doxygen_documentation.cmake)

# enable coding style test
include(cmake/enable_code_style_check.cmake)

# Install the library and the executables
install( TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin LIBRARY DESTINATION lib )

# all include files go into include/PROJECT_NAME
# The exclusion of ${PROJECT_NAME} prevents the recursive installation of the files just being installed.
# The original headers are in include/*.h, the installed ones in include/PROJECT_NAME/*.h.
install(DIRECTORY ${${PROJECT_NAME}_INCLUDE_DIRS} DESTINATION include/ChimeraTK/ApplicationCore
                                                  FILES_MATCHING PATTERN "*.h"
                                                                 PATTERN ".svn" EXCLUDE
                                                                 PATTERN "${PROJECT_NAME}" EXCLUDE)

set(${PROJECT_NAME}_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")
set(${PROJECT_NAME}_LIBRARIES "${ChimeraTK-ControlSystemAdapter_LIBRARIES}")
set(${PROJECT_NAME}_LIBRARY_DIRS "${CMAKE_INSTALL_PREFIX}/lib")
set(${PROJECT_NAME}_CXX_FLAGS "${ChimeraTK-ControlSystemAdapter_CXX_FLAGS}")
set(${PROJECT_NAME}_LINK_FLAGS "${ChimeraTK-ControlSystemAdapter_LINK_FLAGS}")

list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "Boost COMPONENTS system thread chrono filesystem date_time atomic")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
