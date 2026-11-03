#ifndef RHSM_DNF5_PLUGINS_RHSM_UTILS_HPP
#define RHSM_DNF5_PLUGINS_RHSM_UTILS_HPP

#include <filesystem>
#include <string>
#include <varlink.h>

constexpr const char * CONSUMER_CERT_DIR = "/etc/pki/consumer/";
constexpr const char * ENTITLEMENT_CERT_DIR = "/etc/pki/entitlement/";
constexpr const char * RELEASEVER_FILE = "/etc/dnf/vars/releasever";
constexpr const char * REDHAT_REPO_FILE = "/etc/yum.repos.d/redhat.repo";

constexpr const char * RHSM_HOST_CONFIG_DIR = "/etc/rhsm-host";
constexpr const char * ENTITLEMENT_HOST_CERT_DIR = "/etc/pki/entitlement-host";

#define EPOLL_MAX_EVENTS 10

// Wait 10 seconds for response (value in milliseconds)
#define EPOLL_WAIT_TIMEOUT_MS 10000

// Wait 30 seconds for all responses
#define TOTAL_WAIT_TIMEOUT_MS 30000

#define VARLINK_SOCKET_FILEPATH "unix:/run/rhc/com.redhat.rhc"

/// Structure holding information about refreshing:
/// * entitlement certificate
/// * repo overrides
/// * releasever
typedef struct RefreshInfo {
    bool entitlement_cert_refresh_finished;
    bool entitlement_cert_refresh_success;
    std::string entitlement_cert_file_path;
    std::string entitlement_key_file_path;
    bool repo_overrides_refresh_finished;
    bool repo_overrides_refresh_success;
    bool releasever_refresh_finished;
    bool releasever_refresh_success;
} RefreshInfo;

/// RAII wrapper holding information about the current Varlink context.
///
/// The constructor establishes the connection to the Varlink service and sets
/// up the epoll instance used to wait for replies. If any step fails, the
/// constructor throws std::runtime_error after releasing any resources it has
/// already acquired. The destructor releases the epoll instance and the Varlink
/// connection, so callers do not have to clean up explicitly, even when an
/// exception propagates out of one of the call_* methods.
class VarlinkCTX {
public:
    /// Establish the Varlink connection and set up epoll.
    /// Throws std::runtime_error when the context cannot be initialized.
    VarlinkCTX();
    ~VarlinkCTX();

    // The context owns raw OS resources, so it must not be copied or moved.
    VarlinkCTX(const VarlinkCTX &) = delete;
    VarlinkCTX & operator=(const VarlinkCTX &) = delete;
    VarlinkCTX(VarlinkCTX &&) = delete;
    VarlinkCTX & operator=(VarlinkCTX &&) = delete;

    /// Information about the refresh operations, shared with the reply callbacks
    /// as their userdata. The caller owns the pointed-to object; this context
    /// only references it.
    RefreshInfo *refresh_info = nullptr;

    /// Call the Varlink method that refreshes entitlement certificate(s).
    /// Throws std::runtime_error when the call cannot be dispatched.
    void call_refresh_entitlement_certificates() const;

    /// Call the Varlink method that writes content overrides to
    /// repo override files
    /// Throws std::runtime_error when the call cannot be dispatched.
    void call_override_write_content(VarlinkArray *content_overrides) const;

    /// Call the Varlink method that downloads repository overrides.
    /// Throws std::runtime_error when the call cannot be dispatched.
    void call_override_download() const;

    /// Call the Varlink method that set the release version.
    /// Throws std::runtime_error when the call cannot be dispatched.
    void call_release_set(const std::string& release) const;

    /// Call the Varlink method that downloads the release version.
    /// Throws std::runtime_error when the call cannot be dispatched.
    void call_release_download() const;

    /// Drive the epoll event loop until all refresh operations have finished.
    /// Throws std::runtime_error when epoll_wait() is interrupted or a reply
    /// cannot be processed.
    void run_event_loop(bool full_refresh) const;

private:
    VarlinkConnection *connection = nullptr;
    int epoll_fd = -1;
    int varlink_fd = -1;
};

/// Generate a random RFC 4122 version 4 UUID in the canonical
/// 8-4-4-4-12 lowercase hyphenated representation
/// (e.g. "83f9c46c-3bb2-4d36-89be-e2099618c0b3").
std::string generate_uuid();

/// Determine the current locale from the environment, following POSIX
/// precedence (LC_ALL, then LC_MESSAGES, then LANG). Returns "C.UTF-8" when
/// none of those variables are set. Does not modify the global locale state.
std::string get_locale();

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

/// Check if the repo file is outdated. The repo file is considered outdated,
/// when it does not exist, or when any entitlement certificate (.pem) in
/// entitlement_cert_dir was modified after the last modification of the repo
/// file. The key files (*-key.pem) are ignored, because each key file is
/// written together with its certificate. It returns false, when there is no
/// entitlement certificate in entitlement_cert_dir.
/// Throws std::runtime_error when the modification time of a file cannot be
/// determined.
bool is_repo_file_outdated(const std::filesystem::path & entitlement_cert_dir,
                           const std::filesystem::path & repo_file);

/// Call Varlink method that tries to refresh entitlement certificate(s),
/// eventually try to get content override and releasever, when full refresh
/// is enabled
/// Throw std::runtime_error if it is not possible to refresh data for content access
void refresh_content(RefreshInfo *refresh_info, bool full_refresh);

#endif // RHSM_DNF5_PLUGINS_RHSM_UTILS_HPP
