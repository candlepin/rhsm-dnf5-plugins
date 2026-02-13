.ONESHELL:
.SHELLFLAGS := -e -c

VERSION := $(shell rpmspec libdnf5-plugins-rhsm.spec --query --queryformat '%{version}')

# The 'build' target is not used during packaging; it is present for upstream development purposes.
.PHONY: build
build: clean
	mkdir build
	cd build
	cmake ../
	make

.PHONY: archive
archive:
	git archive --prefix libdnf5-plugins-rhsm-$(VERSION)/ --format tar.gz HEAD > libdnf5-plugins-rhsm-$(VERSION).tar.gz

.PHONY: srpm
srpm: archive
	rpmbuild --define "_sourcedir $$(pwd)" -bs libdnf5-plugins-rhsm.spec

.PHONY: rpm
rpm: archive
	rpmbuild --define "_sourcedir $$(pwd)" -bb libdnf5-plugins-rhsm.spec

# The 'clean' target removes build artifacts.
.PHONY: clean
clean:
	rm -rf build
	rm -f libdnf5-plugins-rhsm-*.tar*
