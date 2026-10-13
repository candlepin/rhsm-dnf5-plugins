#include "rhsm_utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <varlink.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <sys/epoll.h>
#include <stacktrace>

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
    (void) userdata;
    auto *refresh_info = static_cast<RefreshInfo *>(userdata);

    refresh_info->entitlement_cert_refresh_finished = true;
    if (error) {
        if (strcmp(error, "com.redhat.rhsm.testing.content.CertificateUpToDate") == 0) {
            const char *message;
            if (varlink_object_get_string(parameters, "message", &message) >= 0) {
                refresh_info->entitlement_cert_refresh_success = true;
            } else {
                fprintf(stdout, "Certificate up to date");
            }
        } else {
            throw std::runtime_error(std::format("Error received: {}", error));
        }
    }

    refresh_info->entitlement_cert_refresh_success = true;

    // const char *certificate_path;
    // if (varlink_object_get_string(parameters, "certificate_path", &certificate_path) >= 0) {
    //     std::cout << "Certificate path: " << certificate_path << std::endl;
    // }

    return true;
}

bool refresh_entitlement_certificates(RefreshInfo *refresh_info, const bool force) {
    VarlinkConnection *connection = nullptr;
    VarlinkObject *parameters = nullptr;
    VarlinkObject *metadata = nullptr;
    int epoll_fd = -1;
    int varlink_fd = -1;

    std::ofstream rhsm_log("/tmp/dnf.rhsm.log", std::ios_base::out | std::ios_base::app);

    rhsm_log << std::stacktrace::current() << std::endl;

    rhsm_log << "Refreshing entitlement certificates..." << std::endl;

    // Try to create a connection to the Varlink service
    if (const long res = varlink_connection_new(&connection, "unix:/run/rhc/com.redhat.rhc"); res < 0) {
        throw std::runtime_error(std::format("Failed to connect: {}", varlink_error_string(-res)));
    }

    rhsm_log << "Varlink connection created" << std::endl;

    // Extract the raw connection file descriptor needed for epoll
    varlink_fd = varlink_connection_get_fd(connection);
    if (varlink_fd < 0) {
        varlink_connection_close(connection);
        throw std::runtime_error(std::format("Failed to fetch Varlink file descriptor."));
    }

    rhsm_log << "Varlink file descriptor: " << varlink_fd << std::endl;

    // Create and configure the epoll instance
    epoll_fd = epoll_create1(0); // More recent safe initialization wrapper
    if (epoll_fd == -1) {
        varlink_connection_close(connection);
        throw std::runtime_error("epoll_create1 failed");
    }

    epoll_event ev = {};
    epoll_event events[EPOLL_MAX_EVENTS];

    memset(&ev, 0, sizeof(ev));
    // Monitor the socket for incoming readable data
    ev.events = EPOLLIN;
    // Pack fd into event mapping context
    ev.data.fd = varlink_fd;

    // Bind the Varlink fd to the epoll tracking engine
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, varlink_fd, &ev) == -1) {
        close(epoll_fd);
        varlink_connection_close(connection);
        throw std::runtime_error("epoll_ctl configuration failed");
    }

    rhsm_log << "Bint the Varlink fd to epoll tracking engine" << std::endl;

    varlink_object_new(&parameters);
    varlink_object_new(&metadata);

    const char *user_agent = "dnf5";
    varlink_object_set_string(metadata, "user_agent", user_agent);
    rhsm_log << "User agent: " << "dnf" << std::endl;

    const std::string correlation_id = generate_uuid();
    rhsm_log << "Correlation ID: " << correlation_id << std::endl;
    varlink_object_set_string(metadata, "correlation_id", correlation_id.c_str());

    const std::string locale = get_locale();
    rhsm_log << "Locale: " << locale << std::endl;
    varlink_object_set_string(metadata, "locale", locale.c_str());

    varlink_object_set_object(parameters, "metadata", metadata);
    varlink_object_set_bool(parameters, "force", force);

    rhsm_log << "Calling Varlink method..." << std::endl;

    // Try to call the Varlink method
    const long res = varlink_connection_call(
        connection,
        "com.redhat.rhsm.testing.content.Refresh",
        parameters,
        0,
        refresh_reply_callback,
        refresh_info
        );
    varlink_object_unref(parameters);

    rhsm_log << "Varlink method called with result:" << res << std::endl;

    if (res < 0) {
        close(epoll_fd);
        varlink_connection_free(connection);
        throw std::runtime_error(std::format("Call failed: {}", varlink_error_string(-res)));
    }

    rhsm_log << "Waiting for Varlink result" << std::endl;

    // Drive the epoll event loop
    bool epoll_wait_interrupted = false;
    while (refresh_info->entitlement_cert_refresh_finished == false) {
        // Blocks until an event hits the socket (timeout is set to -1, which means infinite wait)
        const int num_trig_events = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, EPOLL_WAIT_TIMEOUT_MS);

        rhsm_log << "Epoll returned number of events: " << num_trig_events << std::endl;

        if (num_trig_events < 0) {
            epoll_wait_interrupted = true;
            break;
        }

        for (int i = 0; i < num_trig_events; i++) {
            // Check if the event matches our Varlink file descriptor and is ready to read
            if (events[i].data.fd == varlink_fd && (events[i].events & EPOLLIN)) {

                // CRITICAL: Tells libvarlink to parse the buffer stream and route to reply_callback()
                rhsm_log << "Instructing libvarlink to process events" << std::endl;

                if (const long ret = varlink_connection_process_events(connection, events[i].events); ret < 0) {
                    close(epoll_fd);
                    varlink_connection_free(connection);
                    throw std::runtime_error(std::format(
                        "Error processing wire frame data: {}", varlink_error_string(-ret))
                        );
                }
            }
        }
    }


    close(epoll_fd);
    varlink_connection_free(connection);

    if (epoll_wait_interrupted == true){
        rhsm_log << "epoll_wait() was interrupted" << std::endl;
        throw std::runtime_error("epoll_wait interrupted");
    }
    rhsm_log << "Refresh() finished " << std::endl << std::endl;

    return true;
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
