# rhsm-dnf5-plugins
Red Hat Subscription Management (RHSM) libdnf5/DNF5 plugins written in C++

This repository contains the following plugins:

* [x] `productid` (libdnf5 plugin) more information in [./productid/README.md](./productid/README.md)
* [ ] `rhsm` (libdnf5 plugin) TODO
* [ ] `release` (dnf5 plugin) TODO
* [ ] `reporefresh` (dnf5 plugin) TODO

Requirements
------------

Build Dependencies:

* C++ compiler with C++20 support ([GCC](https://gcc.gnu.org/))
* [CMake](https://cmake.org/) (the minimum required version is 3.21)
* [libdnf5](https://github.com/rpm-software-management/dnf5)
* [OpenSSL](https://www.openssl.org/)
* [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
* [PkgConf](http://pkgconf.org/)

On Fedora, install the build dependencies with:

```console
$ sudo dnf builddep ./libdnf5-plugins-rhsm.spec
```

Testing Dependencies:

* [GoogleTest](https://google.github.io/googletest/) (for unit tests)
* [Behave](https://github.com/behave/behave) (for integration tests)

On Fedora, install the test dependencies with:

```console
$ pip install behave
```

Building plugins
----------------

To build plugins, run the following commands:

```console
$ mkdir build
$ cd build
$ cmake ../
$ make
```

To run unit tests:

```console
$ make test
```

To run integration tests:

```console
$ sudo behave
```

To install the plugins on the system, run:

```console
$ sudo make install
```

For more information on testing, see [TESTING.md](./TESTING.md).
