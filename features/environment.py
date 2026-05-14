import os
import shutil
import tempfile

ENTITLEMENT_CERT_DIR = "/etc/pki/entitlement/"
ENTITLEMENT_BACKUP_DIR_PREFIX = "entitlement-backup-"
RELEASEVER_FILE = "/etc/dnf/vars/releasever"
RHSM_HOST_CONFIG_DIR = "/etc/rhsm-host"
ENTITLEMENT_HOST_CERT_DIR = "/etc/pki/entitlement-host"


def before_scenario(context, scenario):
    """Create a per-scenario temporary directory for entitlement cert backup."""
    context.entitlement_backup_dir = tempfile.mkdtemp(prefix=ENTITLEMENT_BACKUP_DIR_PREFIX)


def after_scenario(context, scenario):
    """Restore filesystem state modified by rhsm integration test scenarios."""

    backup_dir = getattr(context, "entitlement_backup_dir", None)
    if backup_dir and os.path.isdir(backup_dir) and os.listdir(backup_dir):
        os.makedirs(ENTITLEMENT_CERT_DIR, exist_ok=True)
        for filename in os.listdir(backup_dir):
            src = os.path.join(backup_dir, filename)
            dst = os.path.join(ENTITLEMENT_CERT_DIR, filename)
            shutil.move(src, dst)
    if backup_dir:
        shutil.rmtree(backup_dir, ignore_errors=True)

    if getattr(context, "_releasever_created", False):
        if os.path.exists(RELEASEVER_FILE):
            os.remove(RELEASEVER_FILE)
        context._releasever_created = False

    if getattr(context, "_container_mode_active", False):
        for dirpath in [RHSM_HOST_CONFIG_DIR, ENTITLEMENT_HOST_CERT_DIR]:
            if os.path.isdir(dirpath):
                shutil.rmtree(dirpath, ignore_errors=True)
        context._container_mode_active = False
