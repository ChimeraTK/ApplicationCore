# CMakeLists.txt for server modules
cmake_minimum_required(VERSION 3.5.1)

message("\nProcess now CMakeLists.txt in ${CMAKE_CURRENT_SOURCE_DIR}")

include_directories(include)
aux_source_directory(src library_sources)
file(GLOB library_headers "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
set(LIBRARY_SOURCE ${LIBRARY_SOURCE} ${library_sources} ${library_headers})

# Build the modules into a shared library so they are only
# instanciated once, if multiple server instances are run
add_library(${PROJECT_NAME}lib SHARED ${library_sources} ${library_headers})
set_target_properties(${PROJECT_NAME}lib PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                    SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME}lib ${ChimeraTK-ApplicationCore_LIBRARIES} ${ADAPTER_LIBRARIES})
