# Note: This CMakeLists.txt is a minimal complete example how to use the
# *installed* application core library. It is not included from the main CMakeLists.txt
# which is used to build ApplicationCore itself.

#! [Full CMakeLists]
#! [project declaration]
project(demo_example)
#! [project declaration]

cmake_minimum_required(VERSION 3.16)

# Use the project template to get the settings required for an application core project
# The Version 01.23.45 does not represent the actual version of the example and is used
# only for demonstration purposes. Please change when using the example as a template.
#! [version numbers]
set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 23)
set(${PROJECT_NAME}_PATCH_VERSION 45)
include(cmake/set_version_numbers.cmake)
#! [version numbers]

#! [cmake module path]
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)
include(cmake/set_default_build_to_release.cmake)
include(cmake/set_default_flags.cmake)
#! [cmake module path]

#! [find dependencies]
# Add the dependencies. We need ApplicationCore and a control system adapter implementation.
find_package(ChimeraTK-ApplicationCore 04.00 REQUIRED)
#! [find dependencies]

#! [select adapter]
# Choose control system adapater implementation to link against based on the ADAPTER cmake argument. When not
# specifying this at the cmake command line, a list of possible options will be presented.
include(cmake/set_control_system_adapter.cmake)
#! [select adapter]

#! [collect sources]
aux_source_directory(${CMAKE_SOURCE_DIR}/src sources)
aux_source_directory(${CMAKE_SOURCE_DIR}/include headers)
aux_source_directory(${CMAKE_SOURCE_DIR}/src_factory factory)
#! [collect sources]

include_directories(${CMAKE_SOURCE_DIR}/include)

include(cmake/add_linter_target.cmake)

#! [server library]
# Server library links all ApplicationModules and ApplicationCore
add_library(${PROJECT_NAME}lib ${sources} ${headers})
target_link_libraries(${PROJECT_NAME}lib ChimeraTK::ChimeraTK-ApplicationCore)
#! [server library]

#! [server executable]
# Server executable: link the server library with the application factory and the chose control system adapter
add_executable(${PROJECT_NAME} ${factory})
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}lib ChimeraTK::SelectedAdapter)
#! [server executable]

#! [xml generator]
# XML generator executable: generate XML application description. Links the server library with the application
# factory, but sets a special C++ preprocessor symbol to put ApplicationCore into the XML generation mode.
add_executable(${PROJECT_NAME}-xmlGenerator ${factory})
target_compile_options(${PROJECT_NAME}-xmlGenerator PRIVATE "-DGENERATE_XML")
target_link_libraries(${PROJECT_NAME}-xmlGenerator PRIVATE ${PROJECT_NAME}lib)
#! [xml generator]

#! [config files]
# copy the (test) config files to the build directory for tests
file(COPY config/ DESTINATION ${PROJECT_BINARY_DIR})
#! [config files]

#! [tests]
# Tests:
# There are no dedicated tests for this demo. But we run the xml generator to
# check that the variable household can successfully be initialised.
# The test will fail if the xml generator crashes, just a smoke test.
enable_testing()
add_test(${PROJECT_NAME}-xmlGenerator ${PROJECT_NAME}-xmlGenerator)
#! [tests]

#! [install]
# Installation:
# FIXME: For doocs we need a special treatment when installing to /export/doocs/server (don't install to bin subdirectory, but a directory named like the server). This should go to the project template.
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/export/doocs/server")
  install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}-xmlGenerator RUNTIME DESTINATION ${PROJECT_NAME})
else()
  install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}-xmlGenerator RUNTIME DESTINATION bin)
endif()
#! [install]

# Do not install the config. It is only a test config.
# The real config will come from a config generator and usually depends on the instance.
#! [Full CMakeLists]
