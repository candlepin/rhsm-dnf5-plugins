# setup conditional build
if(NOT WITH_PLUGIN_RHSM)
    return()
endif()

# set gettext domain for translations
add_definitions(-DGETTEXT_DOMAIN=\"rhsm-dnf5-plugins\")

# add your source files
add_library(rhsm MODULE rhsm.cpp rhsm_utils.hpp rhsm_utils.cpp)

# disable the 'lib' prefix in order to create rhsm.so
set_target_properties(rhsm PROPERTIES PREFIX "")

# link the libdnf5 library and OpenSSL (for certificate expiry checks)
target_link_libraries(rhsm PUBLIC dnf5 PkgConfig::OPENSSL)

# install the plugin into the common libdnf5-plugins location
install(TARGETS rhsm LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/libdnf5/plugins/")
install(FILES "rhsm.conf" DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/dnf/libdnf5-plugins")

# Enable testing
enable_testing()

# Copy test data files to build directory
file(COPY "${PROJECT_SOURCE_DIR}/rhsm/test_data/expired.pem"
        DESTINATION "${PROJECT_BINARY_DIR}/rhsm/test_data/")
file(COPY "${PROJECT_SOURCE_DIR}/rhsm/test_data/valid.pem"
        DESTINATION "${PROJECT_BINARY_DIR}/rhsm/test_data/")

# Unit testing of rhsm utility functions
add_executable(test_rhsm_utils test_rhsm_utils.cpp rhsm_utils.cpp)
target_compile_definitions(test_rhsm_utils PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/rhsm/test_data")
target_link_libraries(test_rhsm_utils gtest PkgConfig::OPENSSL)
add_test(NAME rhsm_utils_unit_tests COMMAND test_rhsm_utils)
