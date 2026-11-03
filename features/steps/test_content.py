import os
import configparser
import requests
import subprocess
import json

from behave import step
import behave.runner

import shutil

from constants import (
    ENTITLEMENT_CERT_DIR,
    RHSM_CONFIG_FILE,
    EXPIRED_ENTITLEMENT_CERT_NAME,
    EXPIRED_ENTITLEMENT_KEY_NAME,
    DNF5_REDHAT_REPOS_OVERRIDE_FILE,
)

@step("expired entitlement certificate is installed")
def step_impl(context: behave.runner.Context):
    """
    Install a self-signed, already-expired entitlement certificate and key into
    /etc/pki/entitlement/ so that the rhsm plugin detects an expired certificate.

    The plugin only reads the notAfter field of the certificate when deciding whether
    it is expired (it does not validate the trust chain), so a self-signed certificate
    with a notAfter date in the past is sufficient.

    Any real entitlement certificates are moved into the per-scenario backup directory
    and restored by the after_scenario hook; the generated expired cert/key pair is
    removed by the same hook.
    :param context: behave context
    :return: None
    """
    os.makedirs(ENTITLEMENT_CERT_DIR, exist_ok=True)

    # Move real entitlement certificates/keys out of the way so that only the expired
    # certificate is present during the scenario. They are restored in after_scenario.
    for file_name in os.listdir(ENTITLEMENT_CERT_DIR):
        if file_name.endswith(".pem"):
            shutil.move(
                os.path.join(ENTITLEMENT_CERT_DIR, file_name),
                os.path.join(context.entitlement_backup_dir, file_name),
            )

    cert_path = os.path.join(ENTITLEMENT_CERT_DIR, EXPIRED_ENTITLEMENT_CERT_NAME)
    key_path = os.path.join(ENTITLEMENT_CERT_DIR, EXPIRED_ENTITLEMENT_KEY_NAME)

    # Generate a self-signed certificate whose validity is entirely in the past.
    subprocess.run(
        [
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
            "-keyout", key_path,
            "-out", cert_path,
            "-subj", "/CN=expired-entitlement-test",
            "-not_before", "20200101000000Z",
            "-not_after", "20200102000000Z",
        ],
        capture_output=True,
        text=True,
        check=True,
    )

    context._expired_cert_installed = True
    context.entitlement_cert_path = cert_path
    context.entitlement_cert_inode = os.stat(cert_path).st_ino


@step("entitlement certificate is installed")
def step_impl(context: behave.runner.Context):
    """
    Check if at least one entitlement certificate is installed. If yes, then store
    entitlement certificate path in the context
    :param context: behave context
    :return: None
    """
    files = os.listdir(ENTITLEMENT_CERT_DIR)
    ent_cert_found = False
    for file_name in files:
        # Skip key files
        if file_name.endswith("-key.pem"):
            continue
        if file_name.endswith(".pem"):
            with open(os.path.join(ENTITLEMENT_CERT_DIR, file_name), "r") as f:
                context.entitlement_cert_path = os.path.join(ENTITLEMENT_CERT_DIR, file_name)
                context.entitlement_cert_inode = os.stat(context.entitlement_cert_path).st_ino
                ent_cert_found = True
                break

    if not ent_cert_found:
        raise Exception("No entitlement certificate found")


@step("entitlement certificate is regenerated on candlepin server")
def step_impl(context: behave.runner.Context):
    """
    Try to enforce a regenerating entitlement certificate on the candlepin server. This could
    be tested only against locally running candlepin server, where we have admin access.
    :param context: behave context
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

    # Make REST API call to regenerate certificates
    url = f"https://{hostname}:{port}{prefix}/consumers/{consumer_uuid}/certificates?lazy_regen=false"
    response = requests.put(url, auth=('admin', 'admin'), verify=False)
    response.raise_for_status()


@step("entitlement certificate has different name and i-node")
def step_impl(context: behave.runner.Context):
    """
    Check if the entitlement certificate has changed name and i-node
    :param context: behave context
    :return: None
    """
    files = os.listdir(ENTITLEMENT_CERT_DIR)
    ent_cert_path = None
    ent_cert_inode = None
    for file_name in files:
        # Skip key files
        if file_name.endswith("-key.pem"):
            continue
        if file_name.endswith(".pem"):
            ent_cert_path = os.path.join(ENTITLEMENT_CERT_DIR, file_name)
            ent_cert_inode = os.stat(ent_cert_path).st_ino
            break

    if not ent_cert_path:
        raise Exception("No entitlement certificate found")

    if context.entitlement_cert_path == ent_cert_path:
        raise Exception(f"Entitlement certificate has still the same name: {context.entitlement_cert_path}")

    if context.entitlement_cert_inode == ent_cert_inode:
        raise Exception(f"Entitlement certificate has still the same i-node: {context.entitlement_cert_inode}")


@step("installed entitlement certificate contains '{string}'")
def step_impl(context: behave.runner.Context, string):
    """
    Try to find at least one entitlement certificate and check if it contains '{string}'
    :param context: behave context
    :param string: string expected in the entitlement certificate
    :return: None
    """
    files = os.listdir(ENTITLEMENT_CERT_DIR)
    string_found = False
    for file_name in files:
        # Skip key files
        if file_name.endswith("-key.pem"):
            continue
        if file_name.endswith(".pem"):
            with open(os.path.join(ENTITLEMENT_CERT_DIR, file_name), "r") as f:
                content = f.read()
                if string in content:
                    string_found = True
                    break
    assert string_found, f"Entitlement certificate does not contain '{string}'"


@step("entitlement certificate has still the same name and i-node")
def step_impl(context: behave.runner.Context):
    """
    Check if the entitlement certificate has still the same name and i-node
    :param context: behave context
    :return: None
    """
    files = os.listdir(ENTITLEMENT_CERT_DIR)
    ent_cert_path = None
    ent_cert_inode = None
    for file_name in files:
        # Skip key files
        if file_name.endswith("-key.pem"):
            continue
        if file_name.endswith(".pem"):
            ent_cert_path = os.path.join(ENTITLEMENT_CERT_DIR, file_name)
            ent_cert_inode = os.stat(ent_cert_path).st_ino
            break

    if not ent_cert_path:
        raise Exception("No entitlement certificate found")

    if context.entitlement_cert_path != ent_cert_path:
        raise Exception(f"Entitlement certificate name has changed from {context.entitlement_cert_path} to {ent_cert_path}")

    if context.entitlement_cert_inode != ent_cert_inode:
        raise Exception(f"Entitlement certificate i-node has changed from {context.entitlement_cert_inode} to {ent_cert_inode}")


@step("content override is set server side")
def step_impl(context: behave.runner.Context):
    """
    Send given content override to server
    :param context: behave context
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

    # Make REST API call to set testing conten overrides
    body = context.text
    url = f"https://{hostname}:{port}{prefix}/consumers/{consumer_uuid}/content_overrides"
    response = requests.put(
        url,
        auth=('admin', 'admin'),
        verify=False,
        headers={'Content-Type': 'application/json'},
        data=body
    )
    response.raise_for_status()


@step("redhat repo override file does not exists")
def step_impl(context: behave.runner.Context):
    """
    Ensure that the redhat repo override file /etc/dnf/repos.override.d/98-redhat.repo
    does not exist, so that the scenario can reliably detect that the rhsm plugin
    (re)created it from the server-side content overrides.
    :param context: behave context
    :return: None
    """
    if os.path.exists(DNF5_REDHAT_REPOS_OVERRIDE_FILE):
        os.remove(DNF5_REDHAT_REPOS_OVERRIDE_FILE)
    assert not os.path.exists(DNF5_REDHAT_REPOS_OVERRIDE_FILE)


@step("redhat repo override file contains")
def step_impl(context: behave.runner.Context):
    """
    Verify that the redhat repo override file /etc/dnf/repos.override.d/98-redhat.repo
    contains the expected content passed as a doc string.
    :param context: behave context
    :return: None
    """
    assert os.path.exists(DNF5_REDHAT_REPOS_OVERRIDE_FILE), (
        f"Repo override file {DNF5_REDHAT_REPOS_OVERRIDE_FILE} does not exist"
    )
    with open(DNF5_REDHAT_REPOS_OVERRIDE_FILE, "r") as override_file:
        content = override_file.read()
    assert context.text in content, (
        f"Repo override file {DNF5_REDHAT_REPOS_OVERRIDE_FILE} does not contain "
        f"expected content:\n{context.text}\n--- actual content ---\n{content}"
    )