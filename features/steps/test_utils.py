import time

from behave import step
import behave.runner


@step("wait '{number}' seconds")
def step_impl(context: behave.runner.Context, number: str):
    """
    Wait a given number of seconds. Could be a float value.
    :param context: behave context
    :param number: number of seconds to wait
    :return: None
    """
    time.sleep(float(number))
