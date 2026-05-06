Integration Tests
=================

This directory contains integration tests for RHSM libdnf5/DNF5 plugins. We use
[Behave](https://github.com/behave/behave) testing framework for writing integration tests.
The generic information about integration tests could be found in `../TESTING.md` file.

Each plugin should have one `*.feature` file containing all scenarios written in natural language. For example `foo`
libdnf5 plugin should have `foo.feature` in this directory. The content of the feature file could look like this:

```gherkin
Feature: Management of productid certificate by libdnf5 plugin productid
  The productid libdnf5 plugin should install product certificate
  from testing repositories to /etc/pki/product and add record
  to "database" in /var/lib/rhsm/productid.json. When the last RPM
  from given repository is removed, then corresponding product
  certificate should be removed, when it is not needed by any other
  repository.


  Scenario: Install one productid certificate for one installed RPM
    Given system is registered against candlepin server
    Given repositories are enabled
      | repo_id                     |
      | never-enabled-content-37060 |
    When rpms are installed from RPM repository
      | rpm_name   |
      | slow-eagle |
    Then productid certificate "37060.pem" is installed in "/etc/pki/product"
    And product database contains
      | repo_id                     | product_id |
      | never-enabled-content-37060 | 37060      |
    Then rpms are removed from system
      | rpm_name   |
      | slow-eagle |
```

The `behave` needs to know what to do with each sentence written in natural language. For example, the `behave` needs
to translate `Given system is registered against candlepin server` to some action. The `Given` is keyword, but the
rest of the sentence `system is registered against candlepin server` has to have some "implementation" in some
file in `./steps` directory, and we can find there one function with a decorator containing this sentence:

```python
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
```

This step can be reused in many scenarios and in many feature files. Thus, if you want to write your
integration tests, then write it first in natural language in `.feature` file. When some steps are missing,
then implement required steps in an existing file in `./steps` directory or introduce a new `.py` file in
this directory.