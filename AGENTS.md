AGENTS.md
=========

This file contains guidance for various coding agents when helping with the development of code
in this repository.

What is included in this repository
-----------------------------------

This repository contains two libdnf5 plugins, and it will contain several dnf5 plugins in the future.
Each plugin is actually a small subproject. All plugins are written in C++ (C++20 standard).

The libdnf5 plugin productid
----------------------------

The `productid` plugin is located in [./productid](./productid) directory, and it is described in
the [productid/README.md](./productid/README.md) file

The libdnf5 plugin rhsm
-----------------------

The `rhsm` plugin is located in [./rhsm](./rhsm) directory, and it is described in the
[rhsm/README.md](./rhsm/README.md) file.

Building of the project
-----------------------

All plugins are built using `cmake`. Building of the project is described in the [README.md](./README.md)
file. We also have static [Makefile](./Makefile) to simplify some basic tasks of building this project

Testing
-------

Testing of all plugins is described in [TESTING.md](./TESTING.md) file.

Each plugin has its own set of unit tests in its own directory. Unit tests can be written only for
code outside the main plugin class because it seems that it would not be possible to mock the environment 
in which plugins are running.

Integration tests are located in the [./features](./features) directory. To be able to run integration tests,
the RPM with plugins has to be built and installed to the system. The libdnf5 plugins cannot be tested alone.
These plugins are triggered by dnf5 or any other client application using libdnf5.

The dnf5 plugins add new sub-commands to `dnf5` CLI tool. These plugins also have to be installed in the
system to be able to test them.

Unit tests and integration tests are triggered as a part of our CI. We use only GitHub actions for our CI
testing. The definition of our CI testing can be found in [./.github/workflows](./.github/workflows).

CI also runs a linting step (see `./.github/workflows/lint.yml`). Before submitting changes, you can check
formatting locally using `clang-format`. The project follows the clang-format style configured in the
repository. Make sure new C++ code passes the linter.

Security Note
-------------

These plugins run **unconfined as root** on production RHEL systems. When modifying or generating code,
be especially careful to avoid memory leaks, buffer overflows, command injection, and other security
vulnerabilities. Do not introduce code that reads or writes files outside expected paths, spawns
subprocesses with user-controlled input, or handles credentials insecurely.

When running new unit tests, all unit tests should be run using `valgrind` to detect potential
memory leaks.

Code Style and Architecture
---------------------------

* All C++ code uses the **C++20** standard.
* Code formatting is enforced by `clang-format`. Run it before committing.
* Each plugin has a main plugin class (following the libdnf5/DNF5 API) and optionally a `utils.cpp` file
  that contains business logic which can be unit tested.
* Logging can be done only in the main plugin class. Any other file with business logic cannot write
  anything to log or console. Thus, if there is any error state, then an exception has to be raised, 
  and then this exception has to be caught in the main class and handled accordingly.
* Unit tests for `utils.cpp` go in `tests_utils.cpp` in the same plugin directory.
* Do not put testable business logic inside the main plugin class — move it to `utils.cpp` instead.

Pull Requests
-------------

* A PR should reference the relevant card ID in the commit body and PR description (e.g., `Card ID: CCT-1234`).
* Each PR should include or update unit tests and integration tests as appropriate.
* Ensure all CI checks pass (build, unit tests, lint) before requesting review.

Git Commits
-----------

* Each git commit should start with a subject briefly describing the content of the commit message.
* The subject should start with the keywords (`fix`, `feat`, `docs`, etc.) described in the
  [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) document.
* The subject and body are separated with a blank line
* The body of the commit should contain more details. Each detail item should start with `* `
* When commit is related to some issue card, then it should be mentioned on the first line of the commit
  body like this: `* Card ID: CCT-1234`
* All commits have to be signed. It means that `git commit` has to be used with CLI options `-s` and `-S`.
* Example of a commit message could look like this:

```
feat: New cool feature

* Card ID: CCT-123
* Implemented a new cool feature in libdnf5 plugin rhsm
* Extended unit tests for this new feature
* Added new integration tests for this new feature

Signed-off-by: Jiri Hnidek <jhnidek@redhat.com>
```