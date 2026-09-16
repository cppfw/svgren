# if the library is compiled by vcpkg during the port build (i.e. during the package installation),
# then we don't need to build unit tests
if(IS_VCPKG_PORT_BUILD)
    return()
endif()

# no unit tests for ios
if(IOS)
    return()
endif()

set(test_srcs)
myci_add_source_files(test_srcs
    DIRECTORY
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/unit/src
    RECURSIVE
)

myci_declare_application(${PROJECT_NAME}-tests
    GUI
    SOURCES
        ${test_srcs}
    RESOURCE_DIRECTORY
        ${CMAKE_CURRENT_LIST_DIR}/../../tests/unit/samples_data
    DEPENDENCIES
        svgren::svgren
        tst::tst
)

myci_declare_test(
    RUN_TARGET
        run-${PROJECT_NAME}-tests
)
