project(TemplateServer)
cmake_minimum_required(VERSION 3.5.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

# Use the project template to get the settings required for an application core project
set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 00)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)
include(cmake/set_default_build_to_release.cmake)

# configure version.h
configure_file(include/version.h.in ${PROJECT_BINARY_DIR}/version/version.h)
include_directories(${PROJECT_BINARY_DIR}/version)

# Add the dependencies. We need ApplicationCore and a control system adapter implementation.
#
# /!\ if you need the DeviceAccess-DoocsBackend, you have to add the dependency _after_
# /!\ selecting the control system adapter, otherwise your server will not work.
#
# Define dependences
include(cmake/add_dependency.cmake)
add_dependency(ChimeraTK-ApplicationCore 03.00 REQUIRED)

# Select the control system adapter
include(cmake/set_control_system_adapter.cmake)

# Define compiler and linker flags - extend compiler flags with flags from dependencies
include(cmake/set_default_flags.cmake)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ChimeraTK-ApplicationCore_CXX_FLAGS}")
set(CMAKE_LINK_FLAGS "-Wl,--no-as-needed ${Adapter_LINK_FLAGS} ${ChimeraTK-ApplicationCore_LINK_FLAGS}")

############################################################################################################
# server library config
############################################################################################################
# set include directories for library
include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB library_headers "${CMAKE_SOURCE_DIR}/include/*.h")
set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include/)
# create lists with source files for library
aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

# server library (to avoid compiling everyting multiple times)
add_library(${PROJECT_NAME}lib SHARED ${library_sources} ${library_headers} ${PROJECT_BINARY_DIR}/version/version.h)
set_target_properties(${PROJECT_NAME}lib PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                    SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME}lib ${ChimeraTK-ApplicationCore_LIBRARIES} ${ADAPTER_LIBRARIES})

############################################################################################################
# server executable config
############################################################################################################
# The server executable. It depends on application core and the control system adapter implementation.
add_executable(${PROJECT_NAME} src_server/ApplicationInstance.cc)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "${CMAKE_LINK_FLAGS}")
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}lib ${ChimeraTK-ApplicationCore_LIBRARIES} ${Adapter_LIBRARIES} )

# We compile the same sources with the GENERATE_XML flag to get an xml generator.
# This one does not depent on a control system adapter implementation.
add_executable(${PROJECT_NAME}-xmlGenerator src_xmlGenerator/XmlGenerator.cc)
set_target_properties( ${PROJECT_NAME}-xmlGenerator PROPERTIES COMPILE_FLAGS "-DGENERATE_XML")
set_target_properties(${PROJECT_NAME}-xmlGenerator PROPERTIES LINK_FLAGS "${ChimeraTK-ApplicationCore_LINK_FLAGS}")
target_link_libraries(${PROJECT_NAME}-xmlGenerator ${PROJECT_NAME}lib ${ChimeraTK-ApplicationCore_LIBRARIES})

############################################################################################################
# server tests config
############################################################################################################
enable_testing()
find_package(Boost COMPONENTS thread system unit_test_framework REQUIRED)

# Create the executables for automated unit testing (new modular tests).
# add one test executable per source file
aux_source_directory(${CMAKE_SOURCE_DIR}/tests/executables_src testExecutables)
foreach( testExecutableSrcFile ${testExecutables})
    #NAME_WE means the base name without path and (longest) extension
    get_filename_component(excutableName ${testExecutableSrcFile} NAME_WE)
    add_executable(${excutableName} ${testExecutableSrcFile})
    target_link_libraries(${excutableName} ${PROJECT_NAME}lib)
    set_target_properties(${excutableName} PROPERTIES LINK_FLAGS "${CMAKE_LINK_FLAGS}")
    add_test(${excutableName} ${excutableName})
endforeach( testExecutableSrcFile )

# Copy the (test) config files to the build directory
file( COPY ${CMAKE_SOURCE_DIR}/config/ DESTINATION ${PROJECT_BINARY_DIR})

############################################################################################################
# server installation config
############################################################################################################
# Installation:
# FIXME: For doocs we need a special treatment when installing to /export/doocs/server (don't install to bin subdirectory, but a directory named like the server). This should go to the project template.
if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/export/doocs/server")
  install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}-xmlGenerator RUNTIME DESTINATION ${PROJECT_NAME})
else()
  install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}-xmlGenerator RUNTIME DESTINATION bin)
endif()
# Do not install the config. It is only a test config.
# The real config will come from a config generator and usually depends on the instance.
