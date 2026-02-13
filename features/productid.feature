Feature: Management of productid certificate by libdnf5 plugin productid

  Scenario: Install one productid certificate for one installed RPM
    Given system is registered against candlepin server
    Given repositories are enabled
      | repo_id                     |
      | never-enabled-content-37060 |
    When rpm "slow-eagle" is installed from RPM repository
    Then productid certificate "37060.pem" is installed in "/etc/pki/product"
    And product database contains
      | repo_id                     | product_id |
      | never-enabled-content-37060 | 37060      |
