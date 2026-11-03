Feature: RHSM plugin info and warning messages
  The rhsm libdnf5 plugin should display appropriate info and warning
  messages during DNF operations based on the system's registration
  status, entitlement certificates, and release version.


  Scenario: Unregistered system displays registration warning
    Given system is not registered
    When dnf5 command is run
    Then command stderr contains "This system is not registered with an entitlement server"
    And command stderr contains "subscription-manager"


  Scenario: Registered system with SCA certificate shows no warnings
    Given system is registered against candlepin server
    When dnf5 command is run
    Then command stderr does not contain "This system is not registered"
    And command stderr does not contain "No SCA entitlement certificate(s) found"


  Scenario: Registered system without entitlement certificates shows warning
    Given system is registered against candlepin server
    And entitlement certificates are removed
    When dnf5 command is run
    Then command stderr contains "No SCA entitlement certificate(s) found"


  @fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=false)
  Scenario: System with release set displays release info
    Given system is registered against candlepin server
    And release is set to "42"
    When dnf5 command is run
    Then command stdout contains "This system has release set to"
    And command stdout contains "it receives updates only for this release"


  Scenario: Container mode displays container info message
    Given system is not registered
    And system operates in container mode
    When dnf5 command is run
    Then command stdout contains "running in container mode"


  @fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=false)
  Scenario: Entitlement certificate is not refreshed, when certificate is up to date
    Given system is registered against candlepin server
    Given entitlement certificate is installed
    When dnf5 command is run
    Then command stderr does not contain "Refreshing entitlement certificate"
    And entitlement certificate has still the same name and i-node


  @fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=false)
  Scenario: Expired entitlement certificate triggers a refresh
    Given system is registered against candlepin server
    And expired entitlement certificate is installed
    Given entitlement certificate is regenerated on candlepin server
    # We have to wait one second here, because If-Modified-Since HTTP header can be sent only in
    # time format defined in RFC 822, which has only second precision. If Refresh() method was
    # called immediately, then it would lead to 304 HTTP code (not modified), because the time
    # of creating old and new certificate with second precision would be the same.
    Then wait '1' seconds
    When dnf5 command is run
    Then command stderr contains "Refreshing expired entitlement certificate"
    And entitlement certificate has different name and i-node
    And installed entitlement certificate contains 'BEGIN CERTIFICATE'
    And installed entitlement certificate contains 'BEGIN ENTITLEMENT DATA'
    And installed entitlement certificate contains 'BEGIN SIGNATURE'

  # Following integration tests are specific for the configuration option
  # "full_refresh" equal to "yes" in /etc/dnf/libdnf5-plugins/rhsm.conf
  # This configuration option is set typically, when system is registered
  # against Satellite

  @fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=true)
  Scenario: Entitlement certificate is not refreshed, when certificate is up to date
    Given system is registered against candlepin server
    Given entitlement certificate is installed
    When dnf5 command is run
    Then command stderr contains "Refreshing entitlement certificate"
    And entitlement certificate has still the same name and i-node


  @fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=true)
  Scenario: Entitlement certificate is refreshed, when certificate has been regenerated on candlepin server
    Given system is registered against candlepin server
    Given entitlement certificate is installed
    Given entitlement certificate is regenerated on candlepin server
    # We have to wait one second here, because If-Modified-Since HTTP header can be sent only in
    # time format defined in RFC 822, which has only second precision. If Refresh() method was
    # called immediately, then it would lead to 304 HTTP code (not modified), because the time
    # of creating old and new certificate with second precision would be the same.
    Then wait '1' seconds
    When dnf5 command is run
    Then command stderr contains "Refreshing entitlement certificate"
    And entitlement certificate has different name and i-node
    And installed entitlement certificate contains 'BEGIN CERTIFICATE'
    And installed entitlement certificate contains 'BEGIN ENTITLEMENT DATA'
    And installed entitlement certificate contains 'BEGIN SIGNATURE'


  @fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=true)
  Scenario: Releasever is set, when releasever is set server side
    Given system is registered against candlepin server
    Given releasever file does not exists
    And release is set to "44" server side
    When dnf5 command is run
    Then command stdout contains "This system has release set to 44"
    And releasever file contains expected content
      """
      44
      """
    And release is unset server side


  @fixture.configure_rhsm_plugin_option(config_option=full_refresh,value=true)
  Scenario: Repo override is set, when content override is set server side
    Given system is registered against candlepin server
    Given redhat repo override file does not exists
    And content override is set server side
       """
       [{"contentLabel": "content-label-37060", "name": "enabled", "value": "1"}]
       """
    When dnf5 command is run
    And redhat repo override file contains
      """
      [content-label-37060]
      enabled=1
      """