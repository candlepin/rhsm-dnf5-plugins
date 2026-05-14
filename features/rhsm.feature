Feature: RHSM plugin info and warning messages
  The rhsm libdnf5 plugin should display appropriate info and warning
  messages during DNF operations based on the system's registration
  status, entitlement certificates, and release version.


  Scenario: Unregistered system displays registration warning
    Given system is not registered
    When dnf5 command is run
    Then command stdout contains "This system is not registered with an entitlement server"
    And command stdout contains "subscription-manager"


  Scenario: Registered system with SCA certificate shows no warnings
    Given system is registered against candlepin server
    When dnf5 command is run
    Then command stdout does not contain "This system is not registered"
    And command stdout does not contain "No SCA entitlement certificate(s) found"


  Scenario: Registered system without entitlement certificates shows warning
    Given system is registered against candlepin server
    And entitlement certificates are removed
    When dnf5 command is run
    Then command stdout contains "No SCA entitlement certificate(s) found"


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

