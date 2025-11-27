# setup conditional build
if(NOT WITH_PLUGIN_PRODUCTID)
    return()
endif()

# set gettext domain for translations
add_definitions(-DGETTEXT_DOMAIN=\"rhsm-dnf5-plugins\")

# add your source files
add_library(productid MODULE productid.cpp
        productdb.cpp
        productdb.hpp)

# disable the 'lib' prefix in order to create template.so
set_target_properties(productid PROPERTIES PREFIX "")

# link the libdnf5 library
target_link_libraries(productid PUBLIC dnf5)

# install the plugin into the common libdnf5-plugins location
install(TARGETS productid LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/libdnf5/plugins/")

# Testing of productdb
add_executable(test_productdb test_productdb.cpp productdb.cpp)
target_link_libraries(test_productdb gtest jsoncpp)
add_test(productdb test_productdb)
