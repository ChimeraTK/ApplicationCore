FIND_PACKAGE(Boost COMPONENTS unit_test_framework REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB test_headers "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/executables_src testExecutables)

if(NOT ${pybind11_FOUND})
  list(REMOVE_ITEM testExecutables "${CMAKE_CURRENT_SOURCE_DIR}/executables_src/testPython.cc")
endif()

foreach(testExecutableSrcFile ${testExecutables})
  # NAME_WE means the base name without path and (longest) extension
  get_filename_component(executableName ${testExecutableSrcFile} NAME_WE)
  add_executable(${executableName} ${testExecutableSrcFile} ${test_headers})

  # do not link against the boost unit test library, the tests are not written for it!
  target_compile_definitions(${executableName} PRIVATE CHIMERATK_INSIDE_APPLICATION_CORE)
  target_link_libraries(${executableName}
    PRIVATE ${PROJECT_NAME} ChimeraTK::ChimeraTK-ControlSystemAdapter PkgConfig::LibXML++ ${HDF5_LIBRARIES}pybind11::embed)
  target_compile_definitions(${executableName} PRIVATE CHIMERATK_INSIDE_APPLICATION_CORE)
  add_test(${executableName} ${executableName})
endforeach(testExecutableSrcFile)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/executables_src/manual manualTestExecutables)

foreach(testExecutableSrcFile ${manualTestExecutables})
  # NAME_WE means the base name without path and (longest) extension
  get_filename_component(executableName ${testExecutableSrcFile} NAME_WE)
  add_executable(${executableName} ${testExecutableSrcFile} ${test_headers})

  # do not link against the boost unit test library, the tests are not written for it!
  target_link_libraries(${executableName}
    PRIVATE ${PROJECT_NAME} ChimeraTK::ChimeraTK-ControlSystemAdapter PkgConfig::LibXML++ ${HDF5_LIBRARIES})

  target_compile_definitions(${executableName} PRIVATE CHIMERATK_INSIDE_APPLICATION_CORE)
endforeach(testExecutableSrcFile)

add_dependencies(testConnectedDestruction testConnectedDestructionRunner)

# copy config files
file(GLOB test_config_files "${CMAKE_CURRENT_SOURCE_DIR}/config/*")

foreach(test_config_file ${test_config_files})
  get_filename_component(test_config_file_nodir ${test_config_file} NAME)
  configure_file(${test_config_file} ${test_config_file_nodir} COPYONLY)
endforeach(test_config_file)

configure_file(${CMAKE_SOURCE_DIR}/xmlschema/application.xsd application.xsd COPYONLY)
