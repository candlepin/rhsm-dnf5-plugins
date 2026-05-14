from behave import *

import os
import json

from common import run_in_context


@given("system is registered against candlepin server")
def step_impl(context):
    """
    Check if the system is registered against the candlepin server. If not, register it.
    :param context: behave context
    :return: None
    """
    cmd = "rhc status --format json"
    run_in_context(context, cmd, can_fail=True)
    result = json.loads(context.cmd_stdout)
    if not result["rhsm_connected"]:
        # TODO: Read credentials from config file
        cmd = "rhc connect --username admin --password admin --organization donaldduck " \
              "--enable-feature content --disable-feature analytics --disable-feature remote-management"
        run_in_context(context, cmd, can_fail=False)


@given("repositories are enabled")
def step_impl(context):
    """
    Enable repositories for the system.
    :param context: behave context
    :return: None
    """
    for row in context.table:
        cmd = f"dnf5 config-manager setopt {row['repo_id']}.enabled=1"
        run_in_context(context, cmd, can_fail=False)


@when('rpms are installed from RPM repository')
def step_impl(context):
    """
    Install RPM packages from the RPM repository.
    :param context: behave context
    :return: None
    """
    rpm_list = []
    for row in context.table:
        rpm_list.append(row['rpm_name'])
    assert len(rpm_list) > 0
    cmd = f"dnf5 install -y {' '.join(rpm_list)}"
    run_in_context(context, cmd, can_fail=False)


@step("rpms are removed from system")
def step_impl(context):
    """
    Remove RPMSs from the system.
    :param context: behave context
    :return: None
    """
    rpm_list = []
    for row in context.table:
        rpm_list.append(row['rpm_name'])
    assert len(rpm_list) > 0
    cmd = f"dnf5 remove -y {' '.join(rpm_list)}"
    run_in_context(context, cmd, can_fail=False)


@then(
    'productid certificate "{product_cert_name}" is installed in "{product_cert_dir_path}"'
)
def step_impl(context, product_cert_name, product_cert_dir_path):
    """
    Check if the productid certificate is installed in the specified directory.
    :param context: behave context
    :param product_cert_name: name of the productid certificate
    :param product_cert_dir_path: path to the directory where the certificate should be installed
    :return: None
    """
    product_cert_path = os.path.join(product_cert_dir_path, product_cert_name)
    assert os.path.exists(product_cert_path)


PRODUCTDB_PATH = "/var/lib/rhsm/productid.json"


@then("product database contains")
def step_impl(contex):
    """
    Check if the product database contains the expected products and repositories.
    :param contex: behave context
    :return: None
    """
    productdb_content = None
    assert os.path.exists(PRODUCTDB_PATH)
    with open(PRODUCTDB_PATH) as f:
        productdb_content = json.loads(f.read())

    assert productdb_content is not None
    assert isinstance(productdb_content, dict)
    for row in contex.table:
        product_id = row["product_id"]
        repo_id = row["repo_id"]
        assert product_id in productdb_content
        repo_list = productdb_content[product_id]
        assert repo_id in repo_list
