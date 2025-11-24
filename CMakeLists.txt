message("Running CMake on rhsm dnf5 plugins...")
cmake_minimum_required(VERSION 3.21)

project(rhsm-dnf5-plugins LANGUAGES CXX C VERSION 1.0.0)

message("Building ${PROJECT_NAME} version ${PROJECT_VERSION}")

option(WITH_PLUGIN_PRODUCTID "Build with libdnf5 productid plugin" ON)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# warnings
add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-Wcast-align -Wformat-nonliteral -Wmissing-format-attribute -Wredundant-decls -Wsign-compare -Wsign-conversion -Wtype-limits -Wuninitialized -Wwrite-strings)
add_compile_options(-Werror=unused-result -Wodr)

# linker options
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # clang doesn't support this option
    add_link_options(-Wl,--as-needed)
endif()

# libdnf5 plugins
add_subdirectory("productid")

enable_testing()
