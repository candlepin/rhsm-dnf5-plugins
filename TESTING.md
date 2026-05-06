Testing of RHSM libdnf5 and DNF5 plugins
========================================
RHSM libdnf5 and dnf5 plugins are a critical part of RHEL 11+ and have to be carefully tested to avoid
memory leaks and security risks, because the code of plugins is run unconfined as root. Each plugin
should have a decent number of unit tests for functions that could be unit tested. Each plugin should
also have a decent number of integration tests covering at least the basic set of user scenarios.

Unit Tests
----------
Each libdnf5 or DNF5 plugin should try to have unit tests when the business logic is part of the code.
The main C++ file of the plugin has to follow libdnf5/DNF5 API, and it contains only one class that is hard
to unit test. For this reason it is recommended to move business logic outside the "main" plugin
C++ file (e.g. `utils.cpp`). If the plugin contains `utils.cpp`, then unit tests for this file should
be written in `tests_utils.cpp`. We use [GoogleTest](https://google.github.io/googletest/) for unit tests.
If you want to introduce new unit tests for your plugin, then follow the existing pattern, or you can
follow [Quickstart for CMake & GoogleTest](https://google.github.io/googletest/quickstart-cmake.html).
To run unit tests manually, use the following commands:

```console
$ mkdir build
$ cd build
$ cmake ../
$ make
$ make test
```

When all the business logic of the plugin is outsourced to systemd service and the plugin calls only Varlink
methods, then it is not necessary to try to have unit tests at any cost. We can have only integration
tests for such a plugin.

Integration Tests
-----------------
We use [Behave](https://github.com/behave/behave) testing framework for writing integration tests. It
means that it uses the concept of behavior-driven development. The Behave allows writing tests in
a natural language style, backed up by Python code.

Integration tests for all plugins are included in directory `./features`. It is necessary to create
RPM of RHSM plugins and install the plugin to the tested system. It means that you have to run the
following commands:

```console
$ make rpm

$ sudo dnf install /path/to/libdnf5-plugins-rhsm-<VERSION>-<RELEASE>.<DIST>.<ARCH>.rpm
```

When the RPM is successfully installed to the system, then it is possible to run all integration tests
with the following command:

```console
$ sudo behave
```

More information about our integration tests could be found in `./features/README.md` file.