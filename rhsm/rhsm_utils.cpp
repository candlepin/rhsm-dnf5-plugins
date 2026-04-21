#include "rhsm_utils.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <openssl/err.h>

#include <openssl/pem.h>
#include <openssl/x509.h>

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

    const ASN1_TIME *not_after = X509_get0_notAfter(cert.get());
    int cmp = X509_cmp_current_time(not_after);

    // cmp == 0 indicates an error
    if (cmp == 0) {
        throw std::runtime_error("Unable to compare ASN1_TIME in certificate: " + cert_path.string());
    }

    // cmp == -1 means the cert time is earlier than current time (expired)
    return cmp == -1;
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
