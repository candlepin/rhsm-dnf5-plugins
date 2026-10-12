#ifndef RHSM_DNF5_PLUGINS_RHSM_UTILS_HPP
#define RHSM_DNF5_PLUGINS_RHSM_UTILS_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

constexpr const char * CONSUMER_CERT_DIR = "/etc/pki/consumer/";
constexpr const char * ENTITLEMENT_CERT_DIR = "/etc/pki/entitlement/";
constexpr const char * RELEASEVER_FILE = "/etc/dnf/vars/releasever";

constexpr const char * RHSM_HOST_CONFIG_DIR = "/etc/rhsm-host";
constexpr const char * ENTITLEMENT_HOST_CERT_DIR = "/etc/pki/entitlement-host";

/// Structure holding information about refreshing:
/// * entitlement certificate
/// * repo overrides
/// * releasever
typedef struct RefreshInfo {
    bool entitlement_cert_refresh_finished;
    bool entitlement_cert_refresh_success;
    // std::string entitlement_cert_file_path;
    // std::string entitlement_key_file_name;
    bool repo_overrides_refresh_finished;
    bool releasever_refresh_finished;
} RefreshInfo;

/// Check if the current process is running inside a container.
/// Detects UBI containers by checking for the presence of RHSM host directories
/// (RHSM_HOST_CONFIG_DIR and ENTITLEMENT_HOST_CERT_DIR)
bool in_container();

/// Check if a consumer certificate (.pem) exists in the given directory.
/// The presence of a consumer certificate indicates the system is registered.
bool has_consumer_certificate(const std::filesystem::path & consumer_cert_dir);

/// Check if any entitlement certificate (.pem) exists in the given directory.
bool has_entitlement_certificates(const std::filesystem::path & entitlement_cert_dir);

/// Reads the certificate in cert_path and returns true if the notAfter date is before the current date (expired).
/// Throws std::runtime_error if the file cannot be opened or the certificate cannot be parsed.
bool is_cert_expired(const std::filesystem::path & cert_path);

/// Read the release version from the given file path.
/// Returns the trimmed first line of the file, or an empty string if the file
/// is empty or contains only whitespace.
/// Throws std::runtime_error if the file does not exist, cannot be opened,
/// or a read error occurs.
std::string get_releasever(const std::filesystem::path & releasever_file);

/// Call Varlink method that tries to refresh entitlement certificate(s)
/// Returns true if the refresh was successful, false otherwise.
bool refresh_entitlement_certificates(RefreshInfo *refresh_info, bool force);

#endif // RHSM_DNF5_PLUGINS_RHSM_UTILS_HPP
