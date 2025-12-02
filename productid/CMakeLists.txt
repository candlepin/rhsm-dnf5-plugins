# setup conditional build
if(NOT WITH_PLUGIN_PRODUCTID)
    return()
endif()

# set gettext domain for translations
add_definitions(-DGETTEXT_DOMAIN=\"rhsm-dnf5-plugins\")

# add your source files
add_library(productid MODULE productid.cpp
        productdb.cpp
        productdb.hpp
        utils.hpp
        utils.cpp)

# disable the 'lib' prefix in order to create template.so
set_target_properties(productid PROPERTIES PREFIX "")

# link the libdnf5 library
target_link_libraries(productid PUBLIC dnf5 jsoncpp PkgConfig::OPENSSL)

# install the plugin into the common libdnf5-plugins location
install(TARGETS productid LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/libdnf5/plugins/")
install(FILES "productid.conf" DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/dnf/libdnf5-plugins")

# Enable testing
enable_testing()

# Following data files are required by our unit tests
file (COPY "${PROJECT_SOURCE_DIR}/productid/test_data/beea371342cde7daf5b1da602a14ef545b0962c58e75f541ed31177bab5d867a-productid.gz"
        DESTINATION "${PROJECT_BINARY_DIR}/productid/test_data/")
file (COPY "${PROJECT_SOURCE_DIR}/productid/test_data/38091.pem"
        DESTINATION "${PROJECT_BINARY_DIR}/productid/test_data/")
file (COPY "${PROJECT_SOURCE_DIR}/productid/test_data/908.pem"
        DESTINATION "${PROJECT_BINARY_DIR}/productid/test_data/")

# Unit testing of productdb
add_executable(test_productdb test_productdb.cpp productdb.cpp)
target_link_libraries(test_productdb gtest dnf5 jsoncpp)
add_test(NAME productdb_unit_tests COMMAND test_productdb)

# Unit testing of utils
add_executable(test_utils test_utils.cpp utils.cpp)
target_link_libraries(test_utils gtest dnf5 PkgConfig::OPENSSL)
add_test(NAME utils_unit_tests COMMAND test_utils)
