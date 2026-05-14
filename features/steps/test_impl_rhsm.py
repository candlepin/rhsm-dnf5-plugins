from behave import given, when, then

import json
import os
import shutil
import subprocess


ENTITLEMENT_CERT_DIR = "/etc/pki/entitlement/"
ENTITLEMENT_BACKUP_DIR = "/tmp/entitlement-backup/"
RELEASEVER_FILE = "/etc/dnf/vars/releasever"


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
    context.cmd = cmd

    if hasattr(context.scenario, "working_dir") and "cwd" not in run_args:
        run_args["cwd"] = context.scenario.working_dir

    context.cmd_exitcode, context.cmd_stdout, context.cmd_stderr = run(cmd, **run_args)

    if not can_fail and context.cmd_exitcode != 0:
        raise AssertionError(
            f'Running command "{cmd}" failed: {context.cmd_exitcode}\n'
            f'stdout: {context.cmd_stdout}\nstderr: {context.cmd_stderr}'
        )
    elif expected_exit_code is not None and expected_exit_code != context.cmd_exitcode:
        raise AssertionError(
            f'Running command "{cmd}" had unexpected exit code: {context.cmd_exitcode}'
        )


@given("system is not registered")
def step_impl(context):
    """
    Ensure the system is not registered. If currently registered, disconnect.
    :param context: behave context
    :return: None
    """
    cmd = "rhc status --format json"
    run_in_context(context, cmd, can_fail=True)
    result = json.loads(context.cmd_stdout)
    if result["rhsm_connected"]:
        cmd = "rhc disconnect"
        run_in_context(context, cmd, can_fail=False)


@given("entitlement certificates are removed")
def step_impl(context):
    """
    Back up and remove all entitlement certificates from /etc/pki/entitlement/.
    Certificates are restored by the after_scenario hook in environment.py.
    :param context: behave context
    :return: None
    """
    if not os.path.isdir(ENTITLEMENT_CERT_DIR):
        return

    os.makedirs(ENTITLEMENT_BACKUP_DIR, exist_ok=True)
    for filename in os.listdir(ENTITLEMENT_CERT_DIR):
        if filename.endswith(".pem"):
            src = os.path.join(ENTITLEMENT_CERT_DIR, filename)
            dst = os.path.join(ENTITLEMENT_BACKUP_DIR, filename)
            shutil.move(src, dst)


@given('release is set to "{release}"')
def step_impl(context, release):
    """
    Write a release version to /etc/dnf/vars/releasever.
    The file is removed by the after_scenario hook in environment.py.
    :param context: behave context
    :param release: release version string
    :return: None
    """
    os.makedirs(os.path.dirname(RELEASEVER_FILE), exist_ok=True)
    with open(RELEASEVER_FILE, "w") as f:
        f.write(release + "\n")
    context._releasever_created = True


@when("dnf5 command is run")
def step_impl(context):
    """
    Run a lightweight dnf5 command that triggers the rhsm plugin's
    post_base_setup hook.
    :param context: behave context
    :return: None
    """
    run_in_context(context, "dnf5 repolist", can_fail=True)


@then('command stdout contains "{text}"')
def step_impl(context, text):
    """
    Assert that command stdout contains the expected text.
    :param context: behave context
    :param text: expected substring
    :return: None
    """
    assert text in context.cmd_stdout, (
        f'Expected stdout to contain "{text}", but got:\n{context.cmd_stdout}'
    )


@then('command stdout does not contain "{text}"')
def step_impl(context, text):
    """
    Assert that command stdout does NOT contain the given text.
    :param context: behave context
    :param text: unexpected substring
    :return: None
    """
    assert text not in context.cmd_stdout, (
        f'Expected stdout NOT to contain "{text}", but got:\n{context.cmd_stdout}'
    )
