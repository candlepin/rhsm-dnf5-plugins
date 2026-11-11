#include "rhsm_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <varlink.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <sys/epoll.h>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

std::string generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);

    // uuid_unparse writes the canonical 8-4-4-4-12 representation plus a
    // terminating NUL, which requires 37 bytes.
    char uuid_str[UUID_STR_LEN];
    uuid_unparse_lower(uuid, uuid_str);

    return std::string(uuid_str);
}

std::string get_locale() {
    // Determine the locale from the environment following POSIX precedence.
    // We intentionally do not call setlocale(LC_ALL, "") here, because that
    // would mutate the global locale state of the whole dnf5 process in which
    // this plugin runs.
    for (const char *const var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        if (const char *const value = std::getenv(var); value != nullptr && value[0] != '\0') {
            return std::string(value);
        }
    }

    // POSIX default locale when nothing is set in the environment.
    return "C.UTF-8";
}

bool in_container() {
    namespace fs = std::filesystem;

    //  If the path exists, we are in a container.
    //
    //  In UBI containers (RHEL, CentOS), paths HOST_CONFIG_DIR and HOST_ENT_CERT_DIR
    //  are symlinks to container's directories:
    //    /etc/rhsm-host            -> /run/secrets/rhsm/
    //    /etc/pki/entitlement-host -> /run/secrets/etc-pki-entitlement/
    //
    //  The container secrets are bind-mounted to a directory on the host:
    //    /run/secrets (container)  -> /usr/share/rhel/secrets (host)
    //  which is specified in '/usr/share/containers/mounts.conf' (= Podman secret).
    //
    //  The directories inside this host's directory are themselves
    //  symlinks to other host directories populated by subscription-manager:
    //    /usr/share/rhel/secrets/etc-pki-entitlement -> /etc/pki/entitlement
    //    /usr/share/rhel/secrets/redhat.repo         -> /etc/yum.repos.d/redhat.repo
    //    /usr/share/rhel/secrets/rhsm                -> /etc/rhsm
    //
    //  If the container secrets exist, the system is considered to be a container:
    //    /etc/rhsm-host/            exists
    //    /etc/pki/entitlement-host/ exists and is not empty

    return fs::is_directory(RHSM_HOST_CONFIG_DIR) && fs::is_directory(ENTITLEMENT_HOST_CERT_DIR);
}

// Callback method called when Varlink method call returns object with response
static long refresh_reply_callback(VarlinkConnection *connection,
                           const char *error,
                           VarlinkObject *parameters,
                           uint64_t flags,
                           void *userdata) {
    (void) connection;
    (void) flags;
    const auto *ctx = static_cast<VarlinkCTX *>(userdata);
    RefreshInfo *refresh_info = ctx->refresh_info;

    refresh_info->entitlement_cert_refresh_finished = true;

    if (error) {
        if (strcmp(error, "com.redhat.rhsm.testing.content.CertificateUpToDate") == 0) {
            refresh_info->entitlement_cert_up_to_date = true;
        } else {
            throw std::runtime_error(std::format("Error received: {}", error));
        }
    }

    // The reply carries a "result" object holding the paths of the refreshed
    // entitlement certificate and its key. Parse them and store them in
    // refresh_info so the caller knows which files were written.
    VarlinkObject *result = nullptr;
    if (varlink_object_get_object(parameters, "result", &result) >= 0) {
        const char *certificate_path = nullptr;
        if (varlink_object_get_string(result, "certificate_path", &certificate_path) >= 0 &&
            certificate_path != nullptr) {
            refresh_info->entitlement_cert_file_path = certificate_path;
        }

        const char *key_path = nullptr;
        if (varlink_object_get_string(result, "key_path", &key_path) >= 0 &&
            key_path != nullptr) {
            refresh_info->entitlement_key_file_path = key_path;
        }
    }

    return true;
}

// Callback method called when Varlink method call returns object with response
static long write_override_reply_callback(VarlinkConnection *connection,
                           const char *error,
                           VarlinkObject *parameters,
                           uint64_t flags,
                           void *userdata) {
    (void) connection;
    (void) flags;
    (void) userdata;
    (void) parameters;
    const auto *ctx = static_cast<VarlinkCTX *>(userdata);
    RefreshInfo *refresh_info = ctx->refresh_info;

    refresh_info->repo_overrides_refresh_finished = true;

    if (error) {
        throw std::runtime_error(std::format("Error received: {}", error));
    }

    return true;
}

// Callback method called when Varlink method call returns object with response
static long download_override_reply_callback(VarlinkConnection *connection,
                           const char *error,
                           VarlinkObject *parameters,
                           uint64_t flags,
                           void *userdata) {
    (void) connection;
    (void) flags;
    (void) userdata;
    (void) parameters;
    const auto *ctx = static_cast<VarlinkCTX *>(userdata);

    if (error) {
        throw std::runtime_error(std::format("Error received: {}", error));
    }

    VarlinkArray *content_override = nullptr;
    if (varlink_object_get_array(parameters, "content_overrides", &content_override) >= 0) {
        ctx->call_override_write_content(content_override);
    } else {
        throw std::runtime_error(
                "Reply from com.redhat.rhsm.testing.content.override.Download() does not contain \"content_overrides\"");
    }

    return true;
}

// Callback method called when Varlink method call returns object with response
static long set_releasever_reply_callback(VarlinkConnection *connection,
                           const char *error,
                           VarlinkObject *parameters,
                           uint64_t flags,
                           void *userdata) {
    (void) connection;
    (void) flags;
    (void) userdata;
    (void) parameters;
    const auto *ctx = static_cast<VarlinkCTX *>(userdata);
    RefreshInfo *refresh_info = ctx->refresh_info;

    refresh_info->releasever_refresh_finished = true;

    if (error) {
        throw std::runtime_error(std::format("Error received: {}", error));
    }

    return true;
}

// Callback method called when Varlink method call returns object with response
static long download_releasever_reply_callback(VarlinkConnection *connection,
                           const char *error,
                           VarlinkObject *parameters,
                           uint64_t flags,
                           void *userdata) {
    (void) connection;
    (void) flags;
    (void) userdata;
    (void) parameters;
    const auto *ctx = static_cast<VarlinkCTX *>(userdata);
    RefreshInfo *refresh_info = ctx->refresh_info;

    if (error) {
        throw std::runtime_error(std::format("Error received: {}", error));
    }

    const char *releasever;
    if (varlink_object_get_string(parameters, "release", &releasever) >= 0) {
        // Call set release, when releasever is not a null pointer
        if (releasever != nullptr) {
            ctx->call_release_set(releasever);
        } else {
            // When release was not set, then we can finish activity related to release
            refresh_info->releasever_refresh_finished = true;
        }
    } else {
        throw std::runtime_error(
                "Reply from com.redhat.rhsm.testing.content.release.Download() does not contain \"release\"");
    }

    return true;
}

VarlinkCTX::VarlinkCTX() {
    // Try to create a connection to the Varlink service
    if (const long res = varlink_connection_new(&connection, VARLINK_SOCKET_FILEPATH); res < 0) {
        throw std::runtime_error(std::format("Failed to connect: {}", varlink_error_string(-res)));
    }

    // Extract the raw connection file descriptor needed for epoll
    varlink_fd = varlink_connection_get_fd(connection);
    if (varlink_fd < 0) {
        connection = varlink_connection_free(connection);
        throw std::runtime_error(std::format("Failed to fetch Varlink file descriptor."));
    }

    // Create and configure the epoll instance
    epoll_fd = epoll_create1(0); // More recent safe initialization wrapper
    if (epoll_fd == -1) {
        connection = varlink_connection_free(connection);
        throw std::runtime_error("epoll_create1 failed");
    }

    epoll_event ev = {};
    // Monitor the socket for incoming readable data
    ev.events = EPOLLIN;
    // Pack fd into event mapping context
    ev.data.fd = varlink_fd;

    // Bind the Varlink fd to the epoll tracking engine
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, varlink_fd, &ev) == -1) {
        // The destructor is not called for an object whose constructor throws,
        // so the already-acquired resources have to be released here.
        close(epoll_fd);
        epoll_fd = -1;
        connection = varlink_connection_free(connection);
        throw std::runtime_error("epoll_ctl configuration failed");
    }
}

VarlinkCTX::~VarlinkCTX() {
    if (epoll_fd != -1) {
        close(epoll_fd);
        epoll_fd = -1;
    }
    if (connection != nullptr) {
        // varlink_connection_free() fully releases the connection: unlike
        // varlink_connection_close() it guards against an already-freed stream
        // (the peer may have hung up during the event loop, which frees the
        // stream and sets it to NULL), frees the pending callbacks, and frees
        // the connection struct itself.
        connection = varlink_connection_free(connection);
    }
}

// Initialize metadata Varlink argument that is part of all RHSM Varlink methods
static void initialize_metadata(VarlinkObject* metadata)
{
    const char *user_agent = "libdnf5 plugin rhsm";
    varlink_object_set_string(metadata, "user_agent", user_agent);

    const std::string correlation_id = generate_uuid();
    varlink_object_set_string(metadata, "correlation_id", correlation_id.c_str());

    const std::string locale = get_locale();
    varlink_object_set_string(metadata, "locale", locale.c_str());
}

void VarlinkCTX::call_refresh_entitlement_certificates() const
{
    VarlinkObject *parameters = nullptr;
    VarlinkObject *metadata = nullptr;

    varlink_object_new(&parameters);
    varlink_object_new(&metadata);
    initialize_metadata(metadata);
    varlink_object_set_object(parameters, "metadata", metadata);
    // Note: This plugin never force refresh data. There should be dnf5 plugin
    // implementing "refresh" command and this command should have --force
    // CLI option, but "rhsm" libdnf5 plugin itself cannot be triggered with
    // force equal to true.
    // We can set force to true only for testing purpose.
    varlink_object_set_bool(parameters, "force", false);

    // Try to call the Varlink method
    const long cert_refresh_res = varlink_connection_call(
        connection,
        "com.redhat.rhsm.testing.content.Refresh",
        parameters,
        0,
        refresh_reply_callback,
        const_cast<VarlinkCTX *>(this)
    );
    varlink_object_unref(metadata);
    varlink_object_unref(parameters);

    if (cert_refresh_res < 0) {
        throw std::runtime_error(std::format("Call com.redhat.rhsm.testing.content.Refresh() failed: {}",
                                             varlink_error_string(-cert_refresh_res)));
    }
}

void VarlinkCTX::call_override_download() const
{
    VarlinkObject *parameters = nullptr;
    VarlinkObject *metadata = nullptr;

    varlink_object_new(&parameters);
    varlink_object_new(&metadata);
    initialize_metadata(metadata);
    varlink_object_set_object(parameters, "metadata", metadata);

    // Try to call the Varlink method
    const long download_override_res = varlink_connection_call(
        connection,
        "com.redhat.rhsm.testing.content.override.Download",
        parameters,
        0,
        download_override_reply_callback,
        const_cast<VarlinkCTX *>(this)
    );
    varlink_object_unref(metadata);
    varlink_object_unref(parameters);

    if (download_override_res < 0) {
        throw std::runtime_error(std::format("Call com.redhat.rhsm.testing.content.override.Download() failed: {}",
                                             varlink_error_string(-download_override_res)));
    }
}

void VarlinkCTX::call_override_write_content(VarlinkArray *content_overrides) const
{
    VarlinkObject *parameters = nullptr;
    VarlinkObject *metadata = nullptr;

    varlink_object_new(&parameters);
    varlink_object_new(&metadata);
    initialize_metadata(metadata);
    varlink_object_set_array(parameters, "content_overrides", content_overrides);
    varlink_object_set_object(parameters, "metadata", metadata);

    // Try to call the Varlink method
    const long write_override_res = varlink_connection_call(
        connection,
        "com.redhat.rhsm.testing.content.override.WriteContentOverrides",
        parameters,
        0,
        write_override_reply_callback,
        const_cast<VarlinkCTX *>(this)
    );
    varlink_object_unref(metadata);
    varlink_object_unref(parameters);

    if (write_override_res < 0) {
        throw std::runtime_error(std::format("Call com.redhat.rhsm.testing.content.override.WriteContentOverrides() failed: {}",
                                             varlink_error_string(-write_override_res)));
    }
}

void VarlinkCTX::call_release_set(const std::string& release) const
{
    VarlinkObject *parameters = nullptr;
    VarlinkObject *metadata = nullptr;

    varlink_object_new(&parameters);
    varlink_object_new(&metadata);
    initialize_metadata(metadata);
    varlink_object_set_string(parameters, "release", release.c_str());
    varlink_object_set_object(parameters, "metadata", metadata);

    // Try to call the Varlink method
    const long set_release_res = varlink_connection_call(
        connection,
        "com.redhat.rhsm.testing.content.release.SetRelease",
        parameters,
        0,
        set_releasever_reply_callback,
        const_cast<VarlinkCTX *>(this)
    );
    varlink_object_unref(metadata);
    varlink_object_unref(parameters);

    if (set_release_res < 0) {
        throw std::runtime_error(std::format("Call com.redhat.rhsm.testing.content.release.SetRelease() failed: {}",
                                             varlink_error_string(-set_release_res)));
    }
}

void VarlinkCTX::call_release_download() const
{
    VarlinkObject *parameters = nullptr;
    VarlinkObject *metadata = nullptr;

    varlink_object_new(&parameters);
    varlink_object_new(&metadata);
    initialize_metadata(metadata);
    varlink_object_set_object(parameters, "metadata", metadata);

    // Try to call the Varlink method
    const long download_release_res = varlink_connection_call(
        connection,
        "com.redhat.rhsm.testing.content.release.Download",
        parameters,
        0,
        download_releasever_reply_callback,
        const_cast<VarlinkCTX *>(this)
    );
    varlink_object_unref(metadata);
    varlink_object_unref(parameters);

    if (download_release_res < 0) {
        throw std::runtime_error(std::format("Call com.redhat.rhsm.testing.content.release.Download() failed: {}",
                                             varlink_error_string(-download_release_res)));
    }
}

// Drive the epoll event loop
void VarlinkCTX::run_event_loop(const bool full_refresh) const
{
    epoll_event events[EPOLL_MAX_EVENTS];

    // Do not wait for all the replies indefinitely: give up after at most
    // TOTAL_WAIT_TIMEOUT_MS milliseconds measured from now.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(TOTAL_WAIT_TIMEOUT_MS);

    while (refresh_info->entitlement_cert_refresh_finished == false ||
        (full_refresh && (
                refresh_info->repo_overrides_refresh_finished == false ||
                refresh_info->releasever_refresh_finished == false
                )
            )
        )
    {
        // Compute how much of the overall budget is left. When the deadline has
        // already passed, the refresh did not finish in time.
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            throw std::runtime_error(
                std::format("Timed out waiting for Varlink replies after {} ms", TOTAL_WAIT_TIMEOUT_MS));
        }
        const auto remaining_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();

        // Never block longer than the remaining overall budget, and keep the
        // per-call cap so a stalled connection is noticed promptly.
        const int wait_timeout_ms =
            static_cast<int>(std::min<long long>(EPOLL_WAIT_TIMEOUT_MS, remaining_ms));

        // Blocks until an event hits the socket (bounded by wait_timeout_ms)
        const int num_trig_events = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, wait_timeout_ms);
        if (num_trig_events < 0) {
            // A signal can interrupt epoll_wait() while Varlink calls are still
            // outstanding. Bailing out here would abandon those pending
            // operations, which may still mutate certificates or repository
            // state after we stopped waiting for their results. Retry the wait
            // instead; the overall deadline checked above still bounds the loop.
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(std::format(
                "epoll_wait() failed: {}",
                std::error_code(errno, std::generic_category()).message()));
        }

        for (int i = 0; i < num_trig_events; i++) {
            // Only react to events on our Varlink file descriptor
            if (events[i].data.fd != varlink_fd) {
                continue;
            }

            // A hangup or error on the socket means the peer disconnected (or
            // the connection broke) before all replies arrived. Fail promptly
            // instead of spinning on the still-ready socket until the deadline.
            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                throw std::runtime_error(
                    "Varlink connection closed before all replies were received");
            }

            // Ready to read: tell libvarlink to parse the buffer stream and
            // route to reply_callback()
            if (events[i].events & EPOLLIN) {
                if (const long ret = varlink_connection_process_events(connection, events[i].events); ret < 0) {
                    throw std::runtime_error(std::format(
                            "Error processing wire frame data: {}", varlink_error_string(-ret))
                    );
                }
            }
        }
    }
}

void refresh_content(RefreshInfo *refresh_info, const bool full_refresh) {
    refresh_info->entitlement_cert_refresh_finished = false;
    refresh_info->repo_overrides_refresh_finished = false;
    refresh_info->releasever_refresh_finished = false;

    // The constructor connects to the Varlink service and sets up epoll; the
    // destructor releases both, even when one of the calls below throws.
    VarlinkCTX ctx;

    // The context references the caller-owned refresh_info and passes it to the
    // reply callbacks as their userdata.
    ctx.refresh_info = refresh_info;

    // Call Refresh method
    ctx.call_refresh_entitlement_certificates();

    if (full_refresh) {
        // Call Download Override method
        ctx.call_override_download();

        // Call Download Releasever method
        ctx.call_release_download();
    }

    ctx.run_event_loop(full_refresh);
}

bool has_consumer_certificate(const std::filesystem::path &consumer_cert_dir) {
    namespace fs = std::filesystem;

    if (!fs::exists(consumer_cert_dir) || !fs::is_directory(consumer_cert_dir)) {
        return false;
    }
    return std::ranges::any_of(
        fs::directory_iterator(consumer_cert_dir),
        fs::directory_iterator(),
        [](const auto &entry) {
            return entry.path().extension() == ".pem";
        });
}

bool has_entitlement_certificates(const std::filesystem::path &entitlement_cert_dir) {
    namespace fs = std::filesystem;

    if (!fs::exists(entitlement_cert_dir) || !fs::is_directory(entitlement_cert_dir)) {
        return false;
    }
    return std::ranges::any_of(
        fs::directory_iterator(entitlement_cert_dir),
        fs::directory_iterator(),
        [](const auto &entry) {
            return entry.path().extension() == ".pem";
        });
}

bool is_cert_expired(const std::filesystem::path &cert_path) {
    const auto fp = std::unique_ptr<FILE, void(*)(FILE *)>(
        fopen(cert_path.c_str(), "r"),
        [](FILE *f) { if (f) fclose(f); }
    );

    if (fp == nullptr) {
        const std::string error_msg = std::error_code(errno, std::generic_category()).message();
        throw std::runtime_error(std::format("Unable to open file {}: {}", cert_path.string(), error_msg));
    }

    const auto cert = std::unique_ptr<X509, decltype(&X509_free)>(
        PEM_read_X509(fp.get(), nullptr, nullptr, nullptr),
        X509_free
    );

    if (cert == nullptr) {
        unsigned long ssl_err = ERR_get_error();
        const char *reason = ERR_reason_error_string(ssl_err);
        throw std::runtime_error(std::format("Unable to read certificate {}: {}", cert_path.string(), reason));
    }

#if OPENSSL_VERSION_MAJOR > 3
    int error;
    int cmp = X509_check_certificate_times(nullptr, cert.get(), &error);
    if (cmp != 1) {
        switch (error) {
            case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
                throw std::runtime_error("Certificate does not have notBefore field: " + cert_path.string());
            case X509_V_ERR_CERT_NOT_YET_VALID:
                return true;
            case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
                throw std::runtime_error("Certificate does not have notAfter field: " + cert_path.string());
            case X509_V_ERR_CERT_HAS_EXPIRED:
                return true;
            default:
                throw std::runtime_error("Unable to compare certificate times: " + cert_path.string() +
                    "; error code: " + std::to_string(error));
        }
    }
    return false;
#else
    const ASN1_TIME *not_after = X509_get0_notAfter(cert.get());
    const int cmp = X509_cmp_current_time(not_after);
    // cmp == 0 indicates an error
    if (cmp == 0) {
        throw std::runtime_error("Unable to compare ASN1_TIME in certificate: " + cert_path.string());
    }

    // cmp == -1 means the cert time is earlier than current time (expired)
    return cmp == -1;
#endif
}

std::string get_releasever(const std::filesystem::path &releasever_file) {
    namespace fs = std::filesystem;

    if (!fs::exists(releasever_file)) {
        throw std::runtime_error("file does not exist: " + releasever_file.string());
    }

    std::ifstream file(releasever_file);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file: " + releasever_file.string());
    }

    std::string content;
    std::getline(file, content);
    if (file.bad()) {
        throw std::runtime_error("read error on " + releasever_file.string());
    }

    // Trim whitespace
    std::string_view sv = content;
    const auto start = sv.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return std::string();
    }

    sv.remove_prefix(start);
    const auto end = sv.find_last_not_of(" \t\n\r");
    sv.remove_suffix(sv.size() - (end + 1));

    return std::string(sv);
}

bool is_repo_file_outdated(const std::filesystem::path & entitlement_cert_dir,
                           const std::filesystem::path & repo_file) {
    namespace fs = std::filesystem;

    // Without any entitlement certificate there is nothing the repo file
    // could be outdated against.
    if (!fs::exists(entitlement_cert_dir) || !fs::is_directory(entitlement_cert_dir)) {
        return false;
    }

    // Find the modification time of the newest entitlement certificate.
    bool cert_found = false;
    fs::file_time_type newest_cert_time{};
    for (const auto & entry : fs::directory_iterator(entitlement_cert_dir)) {
        if (entry.path().extension() != ".pem") {
            continue;
        }
        // Skip key file
        if (entry.path().stem().string().ends_with("-key")) {
            continue;
        }

        std::error_code ec;
        const auto cert_time = entry.last_write_time(ec);
        if (ec) {
            throw std::runtime_error(std::format(
                "Unable to get modification time of {}: {}", entry.path().string(), ec.message()));
        }

        if (!cert_found || cert_time > newest_cert_time) {
            newest_cert_time = cert_time;
            cert_found = true;
        }
    }

    // There is no entitlement certificate in the directory.
    if (!cert_found) {
        return false;
    }

    // When the repo file does not exist, then it has to be (re)generated.
    if (!fs::exists(repo_file)) {
        return true;
    }

    std::error_code ec;
    const auto repo_time = fs::last_write_time(repo_file, ec);
    if (ec) {
        throw std::runtime_error(std::format(
            "Unable to get modification time of {}: {}", repo_file.string(), ec.message()));
    }

    // The repo file is outdated, when the newest entitlement certificate was
    // written after the last modification of the repo file.
    return newest_cert_time > repo_time;
}

bool is_full_refresh_enabled(const std::string &value) {
    std::string normalized = value;
    std::ranges::transform(normalized, normalized.begin(),
                           [](const unsigned char c) { return std::tolower(c); });
    return normalized == "true" || normalized == "yes" || normalized == "1";
}

bool should_refresh_content(const bool has_expired_certificates,
                            const bool full_refresh,
                            const bool repo_file_outdated) {
    return has_expired_certificates || full_refresh || repo_file_outdated;
}
