libdnf5 plugin rhsm
========================

A libdnf5 plugin that checks Red Hat Subscription Management (RHSM) status and
displays warnings during DNF operations. It hooks into `post_base_setup` and
runs the following checks (as root):

- **Registration** -- warns if no consumer certificate is found in `/etc/pki/consumer/`.
- **Entitlements** -- warns if no SCA entitlement certificates exist in `/etc/pki/entitlement/`, and reports any that have expired.
- **Release version** -- if `/etc/dnf/vars/releasever` is set, logs that updates are pinned to that release.

When the system is registered, the plugin also tries to refresh the data needed
for content access by calling Varlink methods provided by `rhc-server`. The
refresh is triggered when:

- an entitlement certificate has expired,
- `full_refresh` is enabled in `/etc/dnf/libdnf5-plugins/rhsm.conf`, or
- the `redhat.repo` file is older than the newest entitlement certificate in
  `/etc/pki/entitlement/`.

The plugin never writes the `redhat.repo` file itself. As part of the refresh,
`rhc-server` refreshes the entitlement certificates and regenerates
`/etc/yum.repos.d/redhat.repo`. When `full_refresh` is enabled, the repository
overrides and the release version are updated too.

When running inside a UBI container (detected via `/etc/rhsm-host`), the
registration and entitlement-presence checks are skipped since the host
manages those.

Non-root users see a notice that Subscription Management repositories were not
updated.
