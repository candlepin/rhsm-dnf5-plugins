# Notes: if you want to extend your tests, then it could be useful to get
#  the list of RPMs in given repository using following command:
#  $ dnf list --available --repo=never-enabled-content-37060


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


  Scenario: Install one productid certificate for two installed RPM
    Given system is registered against candlepin server
    Given repositories are enabled
      | repo_id                     |
      | never-enabled-content-37060 |
    When rpms are installed from RPM repository
      | rpm_name    |
      | slow-eagle  |
      | tricky-frog |
    Then productid certificate "37060.pem" is installed in "/etc/pki/product"
    And product database contains
      | repo_id                     | product_id |
      | never-enabled-content-37060 | 37060      |
    Then rpms are removed from system
      | rpm_name    |
      | slow-eagle  |
      | tricky-frog |


  Scenario: Install two productid certificates for two installed RPM
    Given system is registered against candlepin server
    Given repositories are enabled
      | repo_id                     |
      | never-enabled-content-37060 |
      | awesomeos-100000000000002   |
    When rpms are installed from RPM repository
      | rpm_name       |
      | slow-eagle     |
      | awesome-rabbit |
    Then productid certificate "37060.pem" is installed in "/etc/pki/product"
    And productid certificate "100000000000002.pem" is installed in "/etc/pki/product"
    And product database contains
      | repo_id                     | product_id      |
      | never-enabled-content-37060 | 37060           |
      | awesomeos-100000000000002   | 100000000000002 |
    When rpms are removed from system
      | rpm_name       |
      | awesome-rabbit |
    Then product database contains
      | repo_id                     | product_id      |
      | never-enabled-content-37060 | 37060           |
    Then rpms are removed from system
      | rpm_name       |
      | slow-eagle     |

  Scenario: Install one productid certificates for two RPMs installed from different repository
    Given system is registered against candlepin server
    Given repositories are enabled
      | repo_id                     |
      | never-enabled-content-37060 |
      | content-label-37060         |
    When rpms are installed from RPM repository
      | rpm_name   |
      | slow-eagle |
      | cool-mouse |
    Then productid certificate "37060.pem" is installed in "/etc/pki/product"
    And product database contains
      | repo_id                     | product_id |
      | never-enabled-content-37060 | 37060      |
      | content-label-37060         | 37060      |
    Then rpms are removed from system
      | rpm_name   |
      | slow-eagle |
      | cool-mouse |
