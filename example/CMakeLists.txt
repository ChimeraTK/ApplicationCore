# Note: This CMakeLists.txt is a minimal complete example how to use the
# *installed* application core library. It is not included from the main CMakeLists.txt
# which is used to build ApplicationCore itself.
PROJECT(demo_example)

CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

# Use the project template to get the settings required for an application core project
set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 00)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)
include(cmake/set_default_build_to_release.cmake)
include(cmake/set_default_flags.cmake)

# Add the dependencies. We need ApplicationCore and a control system adapter implementation.
find_package(ChimeraTK-ApplicationCore 03.00 REQUIRED)
#FIXME: Make the adapter configurable via command line parameter
find_package(ChimeraTK-ControlSystemAdapter-DoocsAdapter 00.10 REQUIRED)

AUX_SOURCE_DIRECTORY(${CMAKE_SOURCE_DIR} demo_sources)

# The server proper. It depends on application core and the control system adapter implementation.
add_executable(${PROJECT_NAME} ${demo_sources})
target_link_libraries(${PROJECT_NAME} 
    PRIVATE ChimeraTK::ChimeraTK-ApplicationCore ChimeraTK::ChimeraTK-ControlSystemAdapter-DoocsAdapter )

# We compile the same sources with the GENERATE_XML flag to get an xml generator.
# This one does not depent on a control system adapter implementation.
add_executable(${PROJECT_NAME}-xmlGenerator ${demo_sources})
target_compile_options(${PROJECT_NAME}-xmlGenerator PRIVATE "-DGENERATE_XML")
target_link_libraries(${PROJECT_NAME}-xmlGenerator PRIVATE ChimeraTK::ChimeraTK-ApplicationCore)

# copy the (test) config files to the build directory for tests
FILE( COPY config/ DESTINATION ${PROJECT_BINARY_DIR})

# Tests:
# There are no dedicated tests for this demo. But we run the xml generator to
# check that the variable household can successfully be initialised.
# The test will fail if the xml generator crashes, just a smoke test.
ENABLE_TESTING()
add_test(${PROJECT_NAME}-xmlGenerator ${PROJECT_NAME}-xmlGenerator)

# Installation:
# FIXME: For doocs we need a special treatment when installing to /export/doocs/server (don't install to bin subdirectory, but a directory named like the server). This should go to the project template.
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/export/doocs/server")
  install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}-xmlGenerator RUNTIME DESTINATION ${PROJECT_NAME})
else()
  install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}-xmlGenerator RUNTIME DESTINATION bin)
endif()
# Do not install the config. It is only a test config.
# The real config will come from a config generator and usually depends on the instance.
