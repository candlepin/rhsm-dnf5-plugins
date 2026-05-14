import os
import shutil

ENTITLEMENT_CERT_DIR = "/etc/pki/entitlement/"
ENTITLEMENT_BACKUP_DIR = "/tmp/entitlement-backup/"
RELEASEVER_FILE = "/etc/dnf/vars/releasever"


def after_scenario(context, scenario):
    """Restore filesystem state modified by rhsm integration test scenarios."""

    if os.path.isdir(ENTITLEMENT_BACKUP_DIR):
        os.makedirs(ENTITLEMENT_CERT_DIR, exist_ok=True)
        for filename in os.listdir(ENTITLEMENT_BACKUP_DIR):
            src = os.path.join(ENTITLEMENT_BACKUP_DIR, filename)
            dst = os.path.join(ENTITLEMENT_CERT_DIR, filename)
            shutil.move(src, dst)
        shutil.rmtree(ENTITLEMENT_BACKUP_DIR, ignore_errors=True)

    if getattr(context, "_releasever_created", False):
        if os.path.exists(RELEASEVER_FILE):
            os.remove(RELEASEVER_FILE)
        context._releasever_created = False
