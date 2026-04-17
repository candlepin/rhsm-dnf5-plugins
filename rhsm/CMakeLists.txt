# setup conditional build
if(NOT WITH_PLUGIN_RHSM)
    return()
endif()

# set gettext domain for translations
add_definitions(-DGETTEXT_DOMAIN=\"rhsm-dnf5-plugins\")

# add your source files
add_library(rhsm MODULE rhsm.cpp)

# disable the 'lib' prefix in order to create rhsm.so
set_target_properties(rhsm PROPERTIES PREFIX "")

# install the plugin into the common libdnf5-plugins location
install(TARGETS rhsm LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/libdnf5/plugins/")
install(FILES "rhsm.conf" DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/dnf/libdnf5-plugins")
