import os
import configparser
import requests
import subprocess
import json

from behave import given, step
import behave.runner

from constants import RELEASEVER_FILE, DEFAULT_PRODUCT_CERT_DIR, PRODUCT_CERT_DIR, RHSM_CONFIG_FILE


@given("releasever file is empty")
def step_impl(context: behave.runner.Context):
    """
    Create empty /etc/dnf/vars/releasever file
    :param context: behave context
    :return: None
    """
    with open(RELEASEVER_FILE, "w") as release_file:
        release_file.write("")


@given("releasever file is deleted")
def step_impl(context: behave.runner.Context):
    """
    Delete releasever file /etc/dnf/vars/releasever, when it exists
    :param context: behave context
    :return: None
    """
    if os.path.exists(RELEASEVER_FILE):
        os.remove(RELEASEVER_FILE)


@step("releasever file contains")
def step_impl(context: behave.runner.Context):
    """
    Create releasever file /etc/dnf/vars/releasever with given content
    :param context: behave context
    :return: None
    """
    with open(RELEASEVER_FILE, "w") as release_file:
        release_file.write(context.text)


@step("releasever file contains expected content")
def step_impl(context: behave.runner.Context):
    """
    Verify that releasever file /etc/dnf/vars/releasever contains expected content
    :param context: behave context
    :return: None
    """
    with open(RELEASEVER_FILE, "r") as release_file:
        assert context.text in release_file.read()


@step("releasever file does not exists")
def step_impl(context: behave.runner.Context):
    """
    Verify that releasever file /etc/dnf/vars/releasever does not exist
    :param context: behave context
    :return: None
    """
    assert not os.path.exists(RELEASEVER_FILE)


@step("system has no default product certificate installed")
def step_impl(context: behave.runner.Context):
    """
    Verify that no product certificate is installed
    :param context: behave context
    :return: None
    """
    assert os.path.exists(DEFAULT_PRODUCT_CERT_DIR), f"Directory {DEFAULT_PRODUCT_CERT_DIR} does not exist"
    assert len(os.listdir(DEFAULT_PRODUCT_CERT_DIR)) == 0, f"Directory {DEFAULT_PRODUCT_CERT_DIR} is not empty"


@step("system has no product certificate installed")
def step_impl(context: behave.runner.Context):
    """
    Verify that no product certificate is installed
    :param context: behave context
    :return: None
    """
    assert os.path.exists(PRODUCT_CERT_DIR), f"Directory {PRODUCT_CERT_DIR} does not exist"
    assert len(os.listdir(PRODUCT_CERT_DIR)) == 0, f"Directory {PRODUCT_CERT_DIR} is not empty"


@step('release is set to "{release}" server side')
def step_impl(context: behave.runner.Context, release: str):
    """
    Set given release server side
    :param context: behave context
    :param release: string with releasever
    :return: None
    """
    config = configparser.ConfigParser()
    config.read(RHSM_CONFIG_FILE)

    hostname = config.get("server", "hostname")
    port = config.get("server", "port")
    prefix = config.get("server", "prefix")

    # Get consumer UUID using D-Bus
    result = subprocess.run(
        ["busctl", "call", "--json=short", "com.redhat.RHSM1", "/com/redhat/RHSM1/Consumer",
         "com.redhat.RHSM1.Consumer", "GetUuid", "s", ""],
        capture_output=True,
        text=True,
        check=True
    )

    # Parse UUID from JSON output
    json_output = json.loads(result.stdout)
    consumer_uuid = json_output["data"][0]

    # Make REST API call to set release
    body = '{"releaseVer": "' + release + '"}'
    url = f"https://{hostname}:{port}{prefix}/consumers/{consumer_uuid}"
    response = requests.put(
        url,
        auth=('admin', 'admin'),
        verify=False,
        headers={'Content-Type': 'application/json'},
        data=body
    )
    response.raise_for_status()

@step('release is unset server side')
def step_impl(context: behave.runner.Context):
    """
    Set given release server side
    :param context: behave context
    :param release: string with releasever
    :return: None
    """
    config = configparser.ConfigParser()
    config.read(RHSM_CONFIG_FILE)

    hostname = config.get("server", "hostname")
    port = config.get("server", "port")
    prefix = config.get("server", "prefix")

    # Get consumer UUID using D-Bus
    result = subprocess.run(
        ["busctl", "call", "--json=short", "com.redhat.RHSM1", "/com/redhat/RHSM1/Consumer",
         "com.redhat.RHSM1.Consumer", "GetUuid", "s", ""],
        capture_output=True,
        text=True,
        check=True
    )

    # Parse UUID from JSON output
    json_output = json.loads(result.stdout)
    consumer_uuid = json_output["data"][0]

    # Make REST API call to set release
    body = '{"releaseVer": ""}'
    url = f"https://{hostname}:{port}{prefix}/consumers/{consumer_uuid}"
    response = requests.put(
        url,
        auth=('admin', 'admin'),
        verify=False,
        headers={'Content-Type': 'application/json'},
        data=body
    )
    response.raise_for_status()