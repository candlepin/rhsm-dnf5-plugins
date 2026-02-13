from behave import *

import subprocess
import os
import json

def run(cmd, shell=True, cwd=None):
    """
    Run a command.
    Return exitcode, stdout, stderr
    """

    proc = subprocess.Popen(
        cmd,
        shell=shell,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        errors="surrogateescape",
    )

    stdout, stderr = proc.communicate()
    return proc.returncode, stdout, stderr


def run_in_context(context, cmd, can_fail=False, expected_exit_code=None, **run_args):
    """
    Run a command in the context of a behave scenario.
    :param context: behave context
    :param cmd: command to run
    :param can_fail: whether the command can fail without raising an error
    :param expected_exit_code: expected exit code of the command
    :param run_args: additional arguments to pass to subprocess.Popen
    :return: None
    """
    if getattr(context, "faketime", None) is not None:
        cmd = 'NO_FAKE_STAT=1 ' + context.faketime + cmd

    if getattr(context, "fake_kernel_release", None) is not None:
        cmd = context.fake_kernel_release + cmd

    if getattr(context, "lc_all", None) is not None:
        cmd = context.lc_all + cmd

    context.cmd = cmd

    if hasattr(context.scenario, "working_dir") and 'cwd' not in run_args:
        run_args['cwd'] = context.scenario.working_dir

    context.cmd_exitcode, context.cmd_stdout, context.cmd_stderr = run(cmd, **run_args)

    if not can_fail and context.cmd_exitcode != 0:
        raise AssertionError('Running command "%s" failed: %s' % (cmd, context.cmd_exitcode))
    elif expected_exit_code is not None and expected_exit_code != context.cmd_exitcode:
        raise AssertionError(
            'Running command "%s" had unexpected exit code: %s' % (cmd, context.cmd_exitcode)
        )

@given('system is registered against candlepin server')
def step_impl(context):
    """
    Check if the system is registered against the candlepin server. If not, register it.
    :param context: behave context
    :return: None
    """
    cmd = "rhc status --format json"
    run_in_context(context, cmd, can_fail=False)
    result = json.loads(context.cmd_stdout)
    if not result["rhsm_connected"]:
        cmd = "rhc connect --username admin --password admin --organization donaldduck"
        run_in_context(context, cmd, can_fail=False)

@given('repositories are enabled')
def step_impl(context):
    """
    Enable repositories for the system.
    :param context: behave context
    :return: None
    """
    for row in context.table:
        cmd = f"dnf5 config-manager setopt {row['repo_id']}.enabled=1"
        run_in_context(context, cmd, can_fail=False)

@when('rpm "{rpm_name}" is installed from RPM repository')
def step_impl(context, rpm_name):
    """
    Install RPM package from RPM repository.
    :param context: behave context
    :param rpm_name: name of the RPM package to install
    :return: None
    """
    cmd = f"dnf5 install -y {rpm_name}"
    run_in_context(context, cmd, can_fail=False)

@then('productid certificate "{product_cert_name}" is installed in "{product_cert_dir_path}"')
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