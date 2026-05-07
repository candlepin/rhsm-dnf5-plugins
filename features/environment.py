"""
This module contains environment setup and teardown functions for the test suite.
"""

import os
import shutil
import tempfile

ENTITLEMENT_CERT_DIR = "/etc/pki/entitlement/"
ENTITLEMENT_BACKUP_DIR_PREFIX = "entitlement-backup-"
RELEASEVER_FILE = "/etc/dnf/vars/releasever"


def before_scenario(context, scenario) -> None:
    """
    This function is executed before each scenario in the test suite.
    Create a per-scenario temporary directory for entitlement cert backup.
    :param context: Context object
    :param scenario: Scenario object
    :return: None
    """
    context.entitlement_backup_dir = tempfile.mkdtemp(
        prefix=ENTITLEMENT_BACKUP_DIR_PREFIX
    )


def after_scenario(context, scenario) -> None:
    """
    This function is executed after each scenario in the test suite.
    Restore filesystem state modified by rhsm integration test scenarios.
    :param context: Context object
    :param scenario: Scenario object
    :return: None
    """

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


def before_step(context, step) -> None:
    """
    This function is executed before each step in the test suite.
    :param context: Context object
    :param step: Step object
    :return: None
    """
    pass


def after_step(context, step) -> None:
    """
    This function is executed after each step in the test suite.
    It checks if the step failed, and if so, it tries to print stdout and stderr
    of the failed process

    :param context: Context object
    :param step: Step object
    :return: None
    """
    if step.status == "failed":
        print(f"Step '{step.name}' failed!")
        if hasattr(context, "cmd_stdout") and context.cmd_stdout:
            print(f"context stdout: {context.cmd_stdout}")
        if hasattr(context, "cmd_stderr") and context.cmd_stderr:
            print(f"context stderr: {context.cmd_stderr}")
        # TODO: Print content of dnf5 log and rhsm log since
        #       the scenario was started
