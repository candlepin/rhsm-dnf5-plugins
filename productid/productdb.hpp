//
// Created by jhnidek on 24.11.25.
//

#ifndef RHSM_DNF5_PLUGINS_PRODUCTDB_H
#define RHSM_DNF5_PLUGINS_PRODUCTDB_H

#include <fstream>
#include <string>
#include <map>
#include <json/json.h>
#include <utility>
#include <filesystem>

#define PRODUCTDB_DIR "/var/lib/rhsm/"
#define PRODUCT_CERT_DIR "/etc/pki/product/"
#define DEFAULT_PRODUCT_CERT_DIR "/etc/pki/product-default/"
#define DEFAULT_PRODUCTDB_FILE "/var/lib/rhsm/productid.json"

/// The object representing record about the RPM repository. It contains
/// little information. It can be extended in the future if needed.
class RepoRecord {
public:
    explicit RepoRecord(const std::string &repo_id) {
        this->repo_id = repo_id;
    }
    std::string repo_id;
};

/// The object representing record about product certificate
class ProductRecord {
public:

    explicit ProductRecord(std::string product_id, std::string product_cert_path) {
        this->product_id = std::move(product_id);
        this->repos = std::map<std::string, RepoRecord>();
        // Check if a given certificate file exists
        if (std::filesystem::exists(product_cert_path)) {
            this->product_cert_path = std::move(product_cert_path);
            this->is_installed = true;
        } else {
            this->product_cert_path = "";
            this->is_installed = false;
        }
    }

    explicit ProductRecord(std::string product_id) {
        this->product_id = std::move(product_id);
        this->repos = std::map<std::string, RepoRecord>();
        this->product_cert_path = "";
        this->is_installed = false;
        // Check if the product cert with the given product ID exists in
        // directory /etc/pki/product or /etc/pki/product-default
        const auto _product_cert_path = PRODUCT_CERT_DIR + this->product_id + ".pem";
        const auto _default_product_cert_path = DEFAULT_PRODUCT_CERT_DIR + this->product_id + ".pem";
        if (std::filesystem::exists(_product_cert_path)) {
            this->product_cert_path = _product_cert_path;
            this->is_installed = true;
        } else if (std::filesystem::exists(_default_product_cert_path)) {
            this->product_cert_path = _default_product_cert_path;
            this->is_installed = true;
        }
    }

    ProductRecord() {
        this->product_id = "";
        this->repos = std::map<std::string, RepoRecord>();
        this->product_cert_path = "";
        this->is_installed = false;
    }

    /// The ID of product certificate
    std::string product_id;

    //// The list of repositories associated with the product certificate
    std::map<std::string, RepoRecord> repos;

    /// Path to the product certificate installed in /etc/pki/product
    /// or /etc/pki/product-default
    std::string product_cert_path;

    /// Is the product cert already installed in /etc/pki/product-default
    /// or in /etc/pki/product?
    bool is_installed;

    bool add_repo_id(const std::string &repo_id);
    bool remove_repo_id(const std::string &repo_id);
    [[nodiscard]] bool has_repo_id(const std::string &repo_id) const;
};

/// This class is used for managing "database" of product certificates
/// and related repositories. The "database" is stored in a simple JSON
/// document in /var/lib/rhsm/productid.json
class ProductDb {
public:
    explicit ProductDb();
    explicit ProductDb(const std::string &path);
    ~ProductDb();
    std::string path;
    std::map<std::string, ProductRecord> products;

    bool read_product_db();
    [[nodiscard]] bool write_product_db() const;
    [[nodiscard]] Json::Value to_json() const;

    bool add_product_id(const std::string& product_id, const std::string& product_cert_path);
    bool remove_product_id(const std::string& product_id);
    [[nodiscard]] bool has_product_id(const std::string& product_id) const;
};

#endif //RHSM_DNF5_PLUGINS_PRODUCTDB_H
