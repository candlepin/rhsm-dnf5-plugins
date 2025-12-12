libdnf5 plugin productid
========================

This plugin provides functionality to manage product IDs within the dnf5 package manager.
It allows for the installation, removal, and verification of product certificates.

This plugin hooks to dnf5 "pre-transaction" and "post-transaction" phases. It instructs dnf5
to download productid certificates when the RPM repository contains productid certificates in the
"pre-transaction" hook. After the certificate is downloaded to `/var/cache/libdnf5`,
the plugin decompresses the certificates, verifies its integrity, and it installs the certificate
to `/etc/pki/product` if needed.

It is necessary to install product certificates to a system to be able to report proper usage
of products. The reporting of product usage is done by another systemd service that is not
part of this project.

Product DB
----------
The plugin also stores information about mapping of repository IDs to product IDs in a product DB file
`/var/lib/rhsm/productid.json`. This file could look like this:

```json
{
  "479": [
    "rhel-11-for-x86_64-baseos-rpms",
    "rhel-11-for-x86_64-appstream-rpms"
  ]
}
```

It is important to write relations between product IDs and repository IDs to the file,
because the plugin is able to get mapping between repository and product certificate only
in the situation when some RPM is installed from a given repository. For example, when
`jsoncpp-devel.x86_64` is installed from `codeready-builder-for-rhel-11-x86_64-rpms`
repository using `dnf install jsoncpp-devel.x86_64`, then productid plugin downloads
new product certificate. The product certificate is installed to `/etc/pki/product/491.pem`
and the `productid.json` is updated to this:

```json
{
  "479": [
    "rhel-11-for-x86_64-baseos-rpms",
    "rhel-11-for-x86_64-appstream-rpms"
  ],
  "491": [
    "codeready-builder-for-rhel-11-x86_64-rpms"
  ]
}
```

On the other side, when some RPM is removed during transaction, then we have to check if all
repositories from product DB are still the active repositories. It means we have to check if
all repositories from product DB have at least one RPM installed. If not, then the repository
is removed from the product DB. If the product certificate has no repository assigned, then
this product certificate is removed from the system, because the system does not consume any
RPM from the given product, but keep in mind that all product certificates in
`/etc/pki/product-default` are considered as protected and cannot be removed.
