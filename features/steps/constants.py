import os

VARLINK_SOCKET = "/run/rhc/com.redhat.rhc"

ENTITLEMENT_CERT_DIR = "/etc/pki/entitlement/"
ENTITLEMENT_BACKUP_DIR_PREFIX = "entitlement-backup-"
RELEASEVER_FILE = "/etc/dnf/vars/releasever"
RHSM_HOST_CONFIG_DIR = "/etc/rhsm-host"
PRODUCT_CERT_DIR = "/etc/pki/product/"
DEFAULT_PRODUCT_CERT_DIR = "/etc/pki/product-default/"
ENTITLEMENT_HOST_CERT_DIR = "/etc/pki/entitlement-host/"
RHC_SERVER_LOG_FILE = "/var/log/rhc/rhc-server.log"
DNF5_REPOS_OVERRIDE_DIR = "/etc/dnf/repos.override.d"
DNF5_REDHAT_REPOS_OVERRIDE_FILE = os.path.join(DNF5_REPOS_OVERRIDE_DIR, "98-redhat.repo")
LIBDNF5_RHSM_CONF_FILE_PATH = "/etc/dnf/libdnf5-plugins/rhsm.conf"
RHSM_CONFIG_FILE = "/etc/rhsm/rhsm.conf"
REDHAT_REPO_FILE = "/etc/yum.repos.d/redhat.repo"

# Names of the self-signed, already-expired entitlement certificate/key pair that is
# installed by the "expired entitlement certificate is installed" step. The serial-like
# stem is chosen to not collide with real candlepin-issued certificates.
EXPIRED_ENTITLEMENT_CERT_NAME = "0000000000000000001.pem"
EXPIRED_ENTITLEMENT_KEY_NAME = "0000000000000000001-key.pem"
