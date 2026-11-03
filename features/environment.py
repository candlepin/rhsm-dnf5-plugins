"""
This module contains environment setup and teardown functions for the test suite.
"""

import os
import re
import shutil
import tempfile

from behave.fixture import use_fixture, use_fixture_by_tag

from fixtures import configure_rhsm_plugin_option, fixture_registry
from steps.constants import (
    EXPIRED_ENTITLEMENT_CERT_NAME,
    EXPIRED_ENTITLEMENT_KEY_NAME,
)

# Matches patterns like:
#   fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=true)
CONFIGURE_RHSM_PLUGIN_OPTION_TAG_PATTERN = re.compile(
    r"fixture\.configure_rhsm_plugin_option\((.+)\)"
)

ENTITLEMENT_CERT_DIR = "/etc/pki/entitlement/"
ENTITLEMENT_BACKUP_DIR_PREFIX = "entitlement-backup-"
RELEASEVER_FILE = "/etc/dnf/vars/releasever"
RHSM_HOST_CONFIG_DIR = "/etc/rhsm-host"
ENTITLEMENT_HOST_CERT_DIR = "/etc/pki/entitlement-host"


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

    if getattr(context, "_expired_cert_installed", False):
        for filename in (EXPIRED_ENTITLEMENT_CERT_NAME, EXPIRED_ENTITLEMENT_KEY_NAME):
            filepath = os.path.join(ENTITLEMENT_CERT_DIR, filename)
            if os.path.exists(filepath):
                os.remove(filepath)
        context._expired_cert_installed = False

    if getattr(context, "_releasever_created", False):
        if os.path.exists(RELEASEVER_FILE):
            os.remove(RELEASEVER_FILE)
        context._releasever_created = False

    if getattr(context, "_container_mode_active", False):
        for dirpath in [RHSM_HOST_CONFIG_DIR, ENTITLEMENT_HOST_CERT_DIR]:
            if os.path.isdir(dirpath):
                shutil.rmtree(dirpath, ignore_errors=True)
        context._container_mode_active = False


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


def before_tag(context, tag) -> None:
    """
    This function is executed before each tag in the test suite.
    It is used to activate fixtures based on tags.
    :param context: Context object
    :param tag: Tag string
    :return: None
    """
    # Parameterized fixture that sets a config option in the rhsm plugin config file.
    # The tag encodes the option and value as keyword arguments:
    #   @fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=true)
    match = CONFIGURE_RHSM_PLUGIN_OPTION_TAG_PATTERN.match(tag)
    if match:
        # 1. Extract the raw string inside parentheses, e.g.
        #    "config_option=full_refresh,value=true"
        args_str = match.group(1)

        # 2. Split by comma and separate key=value pairs into a dictionary
        kwargs = {}
        for pair in args_str.split(","):
            if "=" in pair:
                key, val = pair.split("=", 1)
                # Clean up whitespace or stray quotes
                key = key.strip()
                val = val.strip().strip("'\"")
                kwargs[key] = val

        # 3. Inject the fixture dynamically using the unpacked kwargs
        return use_fixture(configure_rhsm_plugin_option, context, **kwargs)

    if tag.startswith("fixture."):
        return use_fixture_by_tag(tag, context, fixture_registry)
    return None