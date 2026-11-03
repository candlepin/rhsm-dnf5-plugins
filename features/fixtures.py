"""
Behave fixtures for the rhsm-dnf5-plugins integration test suite.

Each fixture is a generator function decorated with @fixture.
Setup runs before yield, teardown runs after the scenario ends.
Fixtures are activated via @fixture.<name> tags in feature files
and looked up in fixture_registry by the before_tag hook in
environment.py.
"""

import configparser
import os
import shutil
import tempfile

from steps.constants import LIBDNF5_RHSM_CONF_FILE_PATH

from behave import fixture
import behave.runner

@fixture
def configure_rhsm_plugin_option(context: behave.runner.Context, config_option, value):
    """
    Fixture that set config_option to given value in /etc/dnf/libdnf5-plugins/rhsm.conf
    and it restores original file at the end of the fixture.
    :param context: Context object
    :param config_option: Configuration option
    :param value: Value to set
    :return: None
    """
    # Setup for fixture: Backup original file and set option in the config file
    backup_fd, context.original_rhsm_conf_file_path = tempfile.mkstemp()
    os.close(backup_fd)
    shutil.copyfile(LIBDNF5_RHSM_CONF_FILE_PATH, context.original_rhsm_conf_file_path)

    config = configparser.ConfigParser()
    config.read(LIBDNF5_RHSM_CONF_FILE_PATH)
    if not config.has_section("main"):
        config.add_section("main")
    config.set("main", config_option, value)
    with open(LIBDNF5_RHSM_CONF_FILE_PATH, "w") as config_file:
        config.write(config_file)

    yield context

    # Cleanup of fixture: Restore the original config file
    shutil.copyfile(context.original_rhsm_conf_file_path, LIBDNF5_RHSM_CONF_FILE_PATH)
    os.remove(context.original_rhsm_conf_file_path)


fixture_registry = {
    "fixture.configure_rhsm_plugin_option": configure_rhsm_plugin_option,
}