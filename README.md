# rhsm-dnf5-plugins
Red Hat Subscription Management (RHSM) libdnf5/DNF5 plugins written in C++

This repository contains the following plugins:

* [x] `productid` (libdnf5 plugin) more information in [./productid/README.md](./productid/README.md)
* [ ] `rhsm` (libdnf5 plugin) TODO
* [ ] `release` (dnf5 plugin) TODO
* [ ] `reporefresh` (dnf5 plugin) TODO

Requirements
------------

* C++ compiler with C++20 support ([GCC](https://gcc.gnu.org/))
* [CMake](https://cmake.org/) (the minimum required version is 3.21)
* [libdnf5](https://github.com/rpm-software-management/dnf5)
* [OpenSSL](https://www.openssl.org/)
* [PkgConf](http://pkgconf.org/)

Building plugins
----------------

To build plugins, run the following commands:

```console
$ mkdir build
$ cd build
$ cmake ../
$ make
```

To run unit tests, run::

```console
$ make test
```

To install the plugins on the system, run:

```console
$ sudo make install
```