#include "rhsm_utils.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

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

EntitlementScanResult get_expired_entitlements(const std::filesystem::path &entitlement_cert_dir) {
    namespace fs = std::filesystem;

    std::set<std::string> expired_names;
    std::vector<fs::path> unreadable;

    if (!fs::exists(entitlement_cert_dir) || !fs::is_directory(entitlement_cert_dir)) {
        return {};
    }

    for (const auto &entry: fs::directory_iterator(entitlement_cert_dir)) {
        if (entry.path().extension() != ".pem") {
            continue;
        }
        auto stem = entry.path().stem().string();
        if (stem.ends_with("-key")) {
            continue;
        }

        FILE *fp = fopen(entry.path().c_str(), "r");
        if (fp == nullptr) {
            unreadable.push_back(entry.path());
            continue;
        }

        X509 *cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
        fclose(fp);

        if (cert == nullptr) {
            unreadable.push_back(entry.path());
            continue;
        }

        const ASN1_TIME *not_after = X509_get0_notAfter(cert);
        int cmp = X509_cmp_current_time(not_after);

        // Only process expired certificates (cmp <= 0 means expired or error)
        if (cmp > 0) {
            X509_free(cert);
            continue;
        }

        expired_names.insert(stem);
        X509_free(cert);
    }

    return {.expired = {expired_names.begin(), expired_names.end()}, .unreadable = std::move(unreadable)};
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
