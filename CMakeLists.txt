PROJECT(ChimeraTK-ApplicationCore)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 03)
set(${PROJECT_NAME}_MINOR_VERSION 01)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

option(BUILD_TESTS "Build tests." ON)

# Find the ControlSystemAdapter
FIND_PACKAGE(ChimeraTK-ControlSystemAdapter 02.06 REQUIRED)

# Find the DeviceAccess
FIND_PACKAGE(ChimeraTK-DeviceAccess 03.10 REQUIRED)

# Find the XML parser library libxml++
FIND_PACKAGE(PkgConfig REQUIRED)
set(LIBXML++_VERSION "libxml++-2.6")
PKG_CHECK_MODULES(LibXML++ REQUIRED IMPORTED_TARGET ${LIBXML++_VERSION})
PKG_CHECK_MODULES(glib REQUIRED IMPORTED_TARGET glib-2.0)

# Find BOOST filesystem
# Note: we need to search this before looking for the unit_test_framework, since we must not link against the
#       unit_test_framework library (use header-only)
FIND_PACKAGE(Boost COMPONENTS filesystem date_time chrono system thread REQUIRED)

FIND_PACKAGE(Threads)

include(cmake/set_default_build_to_release.cmake)
include(cmake/set_default_flags.cmake)
include(cmake/enable_code_coverage_report.cmake)

include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB headers "${CMAKE_SOURCE_DIR}/include/*.h")
aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)
set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include/ )

# add generic modules
include_directories(${CMAKE_SOURCE_DIR}/Modules/include)
file(GLOB module_headers "${CMAKE_SOURCE_DIR}/Modules/include/*.h")
set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/Modules/include/)
aux_source_directory(${CMAKE_SOURCE_DIR}/Modules/src library_module_sources)

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
add_library(${PROJECT_NAME} SHARED ${library_sources} ${library_module_sources} ${headers} ${module_headers})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                 SOVERSION ${${PROJECT_NAME}_SOVERSION})
# exported includes are all under ${CMAKE_INSTALL_PREFIX}/include
target_include_directories(${PROJECT_NAME} INTERFACE "$<INSTALL_INTERFACE:include>")
target_link_libraries(${PROJECT_NAME} 
    # put ControlSystemAdapter public since implicit dep DeviceAccess must be public
    PUBLIC ChimeraTK::ChimeraTK-ControlSystemAdapter
    PUBLIC ${Boost_LIBRARIES}
    PRIVATE Threads::Threads PkgConfig::LibXML++ PkgConfig::glib ${HDF5_LIBRARIES})

# do not remove runtime path of the library when installing
set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)

# add a target to generate API documentation with Doxygen
include(cmake/enable_doxygen_documentation.cmake)

# enable coding style test
include(cmake/enable_code_style_check.cmake)

# Install the library and the executables
# this defines architecture-dependent ${CMAKE_INSTALL_LIBDIR}
include(GNUInstallDirs)
install( TARGETS ${PROJECT_NAME} 
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

# all include files go into include/PROJECT_NAME
# The exclusion of ${PROJECT_NAME} prevents the recursive installation of the files just being installed.
# The original headers are in include/*.h, the installed ones in include/PROJECT_NAME/*.h.
install(DIRECTORY ${${PROJECT_NAME}_INCLUDE_DIRS} DESTINATION include/ChimeraTK/ApplicationCore
                                                  FILES_MATCHING PATTERN "*.h"
                                                                 PATTERN ".svn" EXCLUDE
                                                                 PATTERN "${PROJECT_NAME}" EXCLUDE)

# we need the public dependencies so create_cmake_config_files can find them as implicit dependencies
list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "Boost COMPONENTS system thread chrono filesystem date_time;ChimeraTK-ControlSystemAdapter")
# we support our cmake EXPORTS as imported targets
set(PROVIDES_EXPORTED_TARGETS 1)
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
