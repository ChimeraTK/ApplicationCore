FIND_PACKAGE(Boost COMPONENTS unit_test_framework REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB test_headers "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/executables_src testExecutables)
foreach( testExecutableSrcFile ${testExecutables})
  #NAME_WE means the base name without path and (longest) extension
  get_filename_component(executableName ${testExecutableSrcFile} NAME_WE)
  add_executable(${executableName} ${testExecutableSrcFile} ${test_headers})
  # do not link against the boost unit test library, the tests are not written for it!
  target_link_libraries(${executableName} ${PROJECT_NAME} ${ChimeraTK-ControlSystemAdapter_LIBRARIES} ${HDF5_LIBRARIES})
  set_target_properties(${executableName} PROPERTIES LINK_FLAGS "-Wl,-rpath,${PROJECT_BINARY_DIR} ${Boost_LINK_FLAGS} ${ChimeraTK-ControlSystemAdapter_LINK_FLAGS}")
  add_test(${executableName} ${executableName})
endforeach( testExecutableSrcFile )

# copy config files
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_with_push.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_readonly.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test.xlmap DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test2.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test3.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test4.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test5.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/testDataValidity1.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/testDataValidity2.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/testDataValidityPropagation.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/testDataValidityPropagation2.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/test.dmap DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/configReaderDevice.map DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/validConfig.xml DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_SOURCE_DIR}/xmlschema/application.xsd DESTINATION ${PROJECT_BINARY_DIR}/tests)
FILE( COPY ${CMAKE_CURRENT_SOURCE_DIR}/deviceInitScript1.bash ${CMAKE_CURRENT_SOURCE_DIR}/deviceInitScript2.bash DESTINATION ${PROJECT_BINARY_DIR}/tests)

