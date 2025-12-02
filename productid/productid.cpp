#include <libdnf5/base/base.hpp>
#include <libdnf5/conf/const.hpp>
#include <libdnf5/plugin/iplugin.hpp>
#include <libdnf5/utils/fs/temp.hpp>
#include <libdnf5/rpm/package_query.hpp>

#include <filesystem>
#include <iostream>
#include <ranges>
#include <chrono>

#include "productdb.hpp"
#include "utils.hpp"

/// This libdnf5 plugin is triggered during dnf transaction, and it tries to download "productid" metadata
/// from the RPM repository and install it to the system when it is necessary. It also tries to remove
/// product certificates when these product certificates are not needed anymore. Product certificates
/// are installed to /etc/pki/product/ directory. The productid plugin also maintains a productdb database
/// of installed product certificates and related repositories. This file is critical for the correct
/// functionality of this plugin because dnf does not hold information about productid metadata.

using namespace libdnf5;

namespace {

constexpr const char * PLUGIN_NAME{"productid"};
constexpr plugin::Version PLUGIN_VERSION{.major = 1, .minor = 0, .micro = 0};
constexpr PluginAPIVersion REQUIRED_PLUGIN_API_VERSION{.major = 2, .minor = 0};

constexpr const char * attrs[]{"author.name", "author.email", "description", nullptr};
constexpr const char * attrs_value[] {
    "Jiri Hnidek",
    "jhnidek@redhat.com",
    "Automatically download productid certificates from Red Hat repositories."
};

constexpr const char * METADATA_TYPE_PRODUCTID = "productid";


class ProductIdPlugin final : public plugin::IPlugin {
public:
    /// Implement a custom constructor for the new plugin.
    /// This is not necessary when you only need the Base object for your implementation.
    /// Optional to override.
    ProductIdPlugin(plugin::IPluginData & data, ConfigParser & config) : IPlugin(data), config(config) {}

    /// Fill in the API version of your plugin.
    /// This is used to check if the provided plugin API version is compatible with the library's plugin API version.
    /// MANDATORY to override.
    [[nodiscard]] PluginAPIVersion get_api_version() const noexcept override { return REQUIRED_PLUGIN_API_VERSION; }

    /// Enter the name of your new plugin.
    /// This is used in log messages when an action or error related to the plugin occurs.
    /// MANDATORY to override.
    [[nodiscard]] const char * get_name() const noexcept override { return PLUGIN_NAME; }

    /// Fill in the version of your plugin.
    /// This is used in informative and debugging log messages.
    /// MANDATORY to override.
    [[nodiscard]] plugin::Version get_version() const noexcept override { return PLUGIN_VERSION; }

    /// Add custom attributes, such as information about yourself and a description of the plugin.
    /// These can be used to query plugin-specific data through the API.
    /// Optional to override.
    [[nodiscard]] const char * const * get_attributes() const noexcept override { return attrs; }

    const char * get_attribute(const char * attribute) const noexcept override {
        for (size_t i = 0; attrs[i]; ++i) {
            if (std::strcmp(attribute, attrs[i]) == 0) {
                return attrs_value[i];
            }
        }
        return nullptr;
    }

    void repos_configured() override {
        repos_configured_hook();
    }

    void post_transaction(const base::Transaction & transaction) override {
        post_transaction_hook(transaction);
    };

    ConfigParser & config;

private:
    static bool is_number(const std::string &str) {
        return !str.empty() && std::ranges::all_of(str, [](const unsigned char ch) {
            return std::isdigit(ch) != 0;
        });
    }

    void process_all_installed_product_certificates(
        ProductDb & product_db) const;

    // Own logging
    template <typename... Ss>
    void debug_log(std::string_view format, Ss &&... args) const;

    template <typename... Ss>
    void info_log(std::string_view format, Ss &&... args) const;

    template <typename... Ss>
    void warning_log(std::string_view format, Ss &&... args) const;

    template <typename... Ss>
    void error_log(std::string_view format, Ss &&... args) const;

    void process_installed_product_certificates(
        const std::string & dir_filepath,
        ProductDb & product_db) const;

    // Hooks
    void repos_configured_hook() const;

    void remove_inactive_repositories_from_product_db(ProductDb & product_db,
        const std::set<std::string> & active_repos) const;

    void remove_inactive_product_certificates(ProductDb & product_db) const;

    bool install_product_certificate(ProductDb & product_db,
        const std::string & cert_content,
        std::string product_id) const;

    void post_transaction_hook(const base::Transaction &) const;

    [[nodiscard]] std::map<std::string, libdnf5::repo::Repo *> get_transaction_repos(const base::Transaction &transaction) const;

    [[nodiscard]] std::set<std::string> get_active_repos() const;

    [[nodiscard]] bool setup_filesystem() const ;
};

template <typename... Ss>
void ProductIdPlugin::debug_log(std::string_view format, Ss &&... args) const {
    Base & base = this->get_base();
    base.get_logger()->debug("[productid plugin] " + libdnf5::utils::sformat(format, std::forward<Ss>(args)...));
}

template <typename... Ss>
void ProductIdPlugin::info_log(std::string_view format, Ss &&... args) const {
    Base & base = this->get_base();
    base.get_logger()->info("[productid plugin] " + libdnf5::utils::sformat(format, std::forward<Ss>(args)...));
}

template <typename... Ss>
void ProductIdPlugin::warning_log(std::string_view format, Ss &&... args) const {
    Base & base = this->get_base();
    base.get_logger()->warning("[productid plugin] " + libdnf5::utils::sformat(format, std::forward<Ss>(args)...));
}

template <typename... Ss>
void ProductIdPlugin::error_log(std::string_view format, Ss &&... args) const {
    Base & base = this->get_base();
    base.get_logger()->error("[productid plugin] " + libdnf5::utils::sformat(format, std::forward<Ss>(args)...));
}

/// Try to process product certificates from a given directory that have not been loaded to the product_db yet during
/// reading of productid.json. These product certificates could be installed manually, or it is the first time
/// the productid plugin has been run, and the productid.json was just empty, or it even did not exist.
void ProductIdPlugin::process_installed_product_certificates(
    const std::string &dir_filepath,
    ProductDb & product_db) const {
    debug_log("Processing certificates from directory {}", dir_filepath);
    for (const auto &entry: std::filesystem::directory_iterator(dir_filepath)) {
        auto filename = entry.path().filename();
        if (filename.extension() != ".pem") {
            debug_log("The file {} is not a product certificate, skipping", entry.path().string());
            continue;
        }
        auto product_id = filename.stem().string();
        // Skip certificates that don't have numeric product ID
        if (!is_number(product_id)) {
            warning_log(
                "The product certificate {} does not have numeric product ID, skipping",
                filename.string());
            continue;
        }
        debug_log("The product certificate '{}' has product ID: {}", entry.path().string(), product_id);
        if (product_db.has_product_id(product_id)) {
            debug_log("The product certificate '{}' is already in the database, skipping", filename.string());
        } else {
            debug_log("Adding product certificate '{}' to the database", filename.string());
            product_db.products[product_id] = ProductRecord(product_id, entry.path().string());
        }
    }
}

/// Tries to get the list of installed productid certificates in the following directories:
/// * /etc/pki/product-default
/// * /etc/pki/product
void ProductIdPlugin::process_all_installed_product_certificates(
    ProductDb & product_db) const {
    namespace fs = std::filesystem;

    // Try to get product certificates from /etc/pki/product-default and /etc/pki/product directories
    for (const auto * const cert_dir_path : { DEFAULT_PRODUCT_CERT_DIR, PRODUCT_CERT_DIR }) {
        if (fs::exists(cert_dir_path)) {
            process_installed_product_certificates(
                cert_dir_path,
                product_db);
        } else {
            debug_log("Directory {} does not exist, skipping", cert_dir_path);
        }
    }
}

/// Try to remove inactive repositories from the product DB
void ProductIdPlugin::remove_inactive_repositories_from_product_db(ProductDb & product_db,
    const std::set<std::string> & active_repos) const {
    // Try to remove inactive repositories from the product DB
    for ( auto & [product_id, product] : product_db.products ) {
        // We cannot remove inactive repositories directly, because it could invalidate iterator.
        std::vector<std::string> to_erase;
        for (const auto &repo_id: product.repos | std::views::keys) {
            if (!active_repos.contains(repo_id)) {
                to_erase.push_back(repo_id);
            }
        }
        for (const auto &repo_id: to_erase) {
            debug_log("Removing inactive repository '{}' (no installed RPMS) from product '{}' in productdb",
                repo_id, product_id);
            product.remove_repo_id(repo_id);
        }
    }
}

/// Try to remove installed productid certificates when no related repository is active
void ProductIdPlugin::remove_inactive_product_certificates(ProductDb & product_db) const {
    std::map<std::string, std::string> to_erase;
    for ( auto & [product_id, product] : product_db.products ) {
        if (product.repos.empty()) {
            auto product_cert_path = product.product_cert_path;
            if (product_cert_path.starts_with(DEFAULT_PRODUCT_CERT_DIR)) {
                debug_log("Skipping removal of default product certificate: '{}' (no assigned repositories)",
                    product_cert_path);
                continue;
            }
            to_erase[product_id] = product.product_cert_path;
        }
    }
    for (const auto &[product_id, product_cert_path]: to_erase) {
        debug_log("Removing product '{}', because it has no repositories assigned", product_cert_path);
        try {
            std::filesystem::remove(product_cert_path);
        } catch (const std::filesystem::filesystem_error &e) {
            warning_log("Failed to remove product certificate from '{}': {}",
                product_cert_path, e.what());
            continue;
        }
        product_db.remove_product_id(product_id);
        debug_log("Product '{}' removed from productdb", product_cert_path);
    }
}

/// Try to install product certificate to /etc/pki/product
bool ProductIdPlugin::install_product_certificate(ProductDb & product_db,
    const std::string & cert_content,
    std::string product_id) const {
    auto product_cert_filepath = PRODUCT_CERT_DIR + product_id + ".pem";
    debug_log("Installing product certificate '{}' to '{}'",
              product_id, product_cert_filepath);
    try {
        auto file = libdnf5::utils::fs::File(product_cert_filepath, "w", false);
        file.write(cert_content);
    } catch (const std::exception &e) {
        warning_log("Failed to install product certificate to '{}': {}",
                    product_cert_filepath, e.what());
        return false;
    }
    debug_log("Product certificate '{}' installed successfully", product_cert_filepath);

    debug_log("Adding a new product '{}' to productdb", product_id);
    product_db.add_product_id(product_id, product_cert_filepath);
    return true;
}

/// This method tries to return all repositories from the current transaction
std::map<std::string, libdnf5::repo::Repo *> ProductIdPlugin::get_transaction_repos(const base::Transaction &transaction) const {
    // Then try to get another set of repositories from transaction package(s)
    std::map<std::string, libdnf5::repo::Repo *> active_repos;
    auto transaction_pkgs = transaction.get_transaction_packages();
    for (const auto &transaction_pkg : transaction_pkgs) {
        auto pkg = transaction_pkg.get_package();
        auto repo = pkg.get_repo();
        auto repo_id = repo->get_id();
        const auto [it, inserted] = active_repos.try_emplace(repo_id, repo.get());
        if (inserted) {
            debug_log("Transaction repository '{}' added to the set of active repositories", repo_id);
        }
    }
    return active_repos;
}

/// Try to get the set of active repo IDs. It means the repositories that are currently
/// enabled and have installed packages.
std::set<std::string> ProductIdPlugin::get_active_repos() const {
    // First, try to get active repositories from installed packages
    std::set<std::string> active_repos;
    libdnf5::rpm::PackageQuery installed_packages(get_base());
    installed_packages.filter_installed();
    for (const auto &package : installed_packages) {
        active_repos.insert(package.get_from_repo_id());
    }
    return active_repos;
}

/// This plugin needs the existence of several directories. Try to create these directories.
bool ProductIdPlugin::setup_filesystem() const {
    // Try to create the directory where we store the productdb ("database" of product certificates).
    // It is critical to have this directory, because productdb is written to this directory.
    if (std::filesystem::exists(PRODUCTDB_DIR)) {
        debug_log("Directory for productdb {} already exists", PRODUCTDB_DIR);
    } else {
        info_log("Directory {} does not exist, creating it", PRODUCTDB_DIR);
        try {
            std::filesystem::create_directories(PRODUCTDB_DIR);
        } catch (const std::filesystem::filesystem_error &e) {
            error_log(
                "Failed to create directory {}: {}; Exiting", PRODUCTDB_DIR, e.what());
            return false;
        }
        // Other users should not be able to read /var/lib/rhsm
        try {
            std::filesystem::permissions(
                PRODUCTDB_DIR,
                std::filesystem::perms::others_all,
                std::filesystem::perm_options::remove);
        } catch (const std::filesystem::filesystem_error &e) {
            error_log("Failed to set permissions for directory {}: {}", PRODUCTDB_DIR, e.what());
            return false;
        }
        info_log("Directory {} created successfully", PRODUCTDB_DIR);
    }
    // Create directories for product certificates. It is critical to have this directory,
    // because product certificates are installed to this directory.
    // Note: It is not necessary to create a directory for default product certificates,
    // because we never write anything to this directory
    if (std::filesystem::exists(PRODUCT_CERT_DIR)) {
        debug_log("Directory for product certificates {} already exists", PRODUCT_CERT_DIR);
    } else {
        info_log("Directory {} does not exist, creating it", PRODUCT_CERT_DIR);
        try {
            std::filesystem::create_directories(PRODUCT_CERT_DIR);
        } catch (const std::filesystem::filesystem_error &e) {
            error_log(
                "Failed to create directory {}: {}; Exiting", PRODUCT_CERT_DIR, e.what());
            return false;
        }
        info_log("Directory {} created successfully", PRODUCT_CERT_DIR);
        // Other users should be able to read /etc/pki/product; no need to change permissions
        // in this case
    }
    return true;
}

/// This hook method is called before transaction processing starts. We order the dnf to try to
/// download productid metadata.
void ProductIdPlugin::repos_configured_hook() const {
    Base & base = get_base();
    debug_log("Hook repos_configured started");
    debug_log("Order dnf to download additional metadata type: productid");
    base.get_config().get_optional_metadata_types_option().set(METADATA_TYPE_PRODUCTID);
    debug_log("Hook repos_configured finished successfully");
}

/// The management of productid certificates is triggered in this hook method after
/// the transaction is finished. It covers two use cases:
///
/// 1. Try to get downloaded product certificates when the new package is installed. Install this
///    product certificate if needed to /etc/pki/product. Add the product certificate and
///    RPM repository to our product "database" in /var/lib/rhsm/productid.json file
/// 2. When any RPM package is removed, then we have to check if it was the last RPM installed
///    from some "active" repository. The "active repository" means that some RPM package
///    is installed in the system from this given repository. Such a repository is listed
///    in /var/lib/rhsm/productid.json. When the repository is not active anymore, then it has
///    to be removed from the "database". When any product in the "database" has no
///    repository assigned, then a related product certificate is removed from
///    /etc/pki/product and the product is also removed from the "database".
void ProductIdPlugin::post_transaction_hook(const base::Transaction & transaction) const {
    Base & base = get_base();

    debug_log("Hook post_transaction started");
    const auto start_time = std::chrono::high_resolution_clock::now();

    // First, try to create all necessary directories
    if (!setup_filesystem()) {
        debug_log("Hook post_transaction terminated with error");
        return;
    }

    // Get the list of enabled repositories
    repo::RepoQuery repos(base);
    repos.filter_enabled(true);

    debug_log("Number of enabled repositories: {}", repos.size());

    auto product_db = ProductDb();

    // First, try to read the product db file from /var/lib/rhsm/productid.json.
    // If it is not possible to read it, because e.g., this file does not exist yet,
    // then it should not be a problem. The new file will be created at the end
    // of the transaction.
    try {
        if (const auto ret = product_db.read_product_db(); ret) {
            debug_log("Successfully read existing productdb from {}", product_db.path);
        }
    } catch (const std::exception &e) {
        warning_log("Failed to read productdb: {}", e.what());
    }

    // Print warning messages when product DB contains products without valid product certificates.
    // This could happen when the product certificate was manually removed from /etc/pki/product or
    // /etc/pki/product-default
    for (const auto &[product_id, product] : product_db.products) {
        if (!product.is_installed) {
            warning_log("Product '{}' has record in product DB, but related product certificate does not exist",
                product_id);
        }
    }

    // Check if there are any new installed product certificates and load these
    // product certs to the product_db too. This could happen when the product certificate was
    // manually added to /etc/pki/product or /etc/pki/product-default.
    process_all_installed_product_certificates(product_db);

    // Get the dictionary of active repositories from transaction
    auto transaction_repos = get_transaction_repos(transaction);
    debug_log("Number of transaction repositories: {}", transaction_repos.size());

    // Go through all active repositories and try to get paths of downloaded productid certificates.
    // Note: when the transaction is e.g. "remove", then cached metadata is empty, but we will probably
    //       not need cached metadata during removal of packages.
    for (const auto &[repo_id, repo]: transaction_repos) {
        std::string productid_path = repo->get_metadata_path(METADATA_TYPE_PRODUCTID);
        if (productid_path.empty()) {
            debug_log(
                "Repository '{}' does not provide productid metadata; skipping",
                repo_id);
            continue;
        }

        debug_log(
            "The productid certificates of '{}' repository downloaded to: {}",
            repo_id,
            productid_path
            );

        std::string cert_content;
        // Try to decompress the downloaded certificate
        try {
            cert_content = decompress_productid_cert(productid_path);
        } catch (const std::exception &e) {
            warning_log("Failed to decompress productid certificate: {}; skipping", e.what());
            continue;
        }

        if (cert_content.empty()) {
            warning_log("Product certificate '{}' is empty; skipping", productid_path);
            continue;
        }

        // Try to get product ID from certificate
        std::string product_id;
        try {
            product_id = get_product_id_from_cert_content(cert_content);
        } catch (const std::exception &e) {
            warning_log("Failed to get product ID from certificate '{}': {}; skipping", productid_path, e.what());
            continue;
        }

        debug_log("The downloaded product certificate '{}' has product ID: {}", productid_path, product_id);

        // If it is a new product certificate, then try to install it
        if (!product_db.has_product_id(product_id)) {
            if (!install_product_certificate(product_db, cert_content, product_id)) continue;
        } else {
            debug_log("Product certificate '{}' is already installed in: '{}'",
                product_id, product_db.products[product_id].product_cert_path);
        }

        // If the repository hasn't been added yet to the productdb, then assign it to the current product
        if (!product_db.products[product_id].has_repo_id(repo_id)) {
            debug_log("Assigning repository '{}' to product '{}' in productdb", repo_id, product_id);
            product_db.products[product_id].add_repo_id(repo_id);
        } else {
            debug_log("Repository '{}' is already assigned to product '{}' in productdb", repo_id, product_id);
        }
    }

    // Get the dictionary of active repositories
    auto active_repos = get_active_repos();
    /// Extend active repos with active transaction_repos
    std::ranges::transform(transaction_repos,
        std::inserter(active_repos, active_repos.end()),
        [](const auto &pair) { return pair.first; }
    );
    debug_log("Number of active repositories: {}", active_repos.size());

    // TODO: Try to protect disabled repositories that have some "active" RPMs. Removing such
    //       disabled repositories could cause removing of related product certificate despite
    //       the product is still used (RPMs from this product are still installed).

    // Check if it is necessary to remove any repository from the "database" or eventually
    // if it is necessary to remove any product certificate.
    // Note: it is not possible to do the following optimization: Try to remove repository
    // and product certificate only in cases when there was at least one "remove"
    // transaction. Why? RPMs could be also removed using "rpm" command, which does not
    // trigger any libdnf plugin. Thus, we have to check the validity of our "database"
    // at the end of this hook.
    remove_inactive_repositories_from_product_db(product_db, active_repos);
    remove_inactive_product_certificates(product_db);

    debug_log("Writing current productdb to {}", product_db.path);
    try {
        if (product_db.write_product_db()) {
            debug_log("The productdb successfully writen to {}", product_db.path);
        }
    } catch (const std::exception &e) {
        warning_log("Failed to write productdb: {}", e.what());
    }

    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    debug_log("Hook post_transaction finished successfully in {} ms", duration.count());
}

}  // namespace

/// Below is a block of functions with C linkage used for loading the plugin binaries from disk.
/// All of these are MANDATORY to implement.

/// Return the plugin API version.
PluginAPIVersion libdnf_plugin_get_api_version(void) {
    return REQUIRED_PLUGIN_API_VERSION;
}

/// Return the plugin name.
const char * libdnf_plugin_get_name(void) {
    return PLUGIN_NAME;
}

/// Return the plugin version.
plugin::Version libdnf_plugin_get_version(void) {
    return PLUGIN_VERSION;
}

/// Return the instance of the implemented plugin.
plugin::IPlugin * libdnf_plugin_new_instance(
    [[maybe_unused]] LibraryVersion library_version, plugin::IPluginData & data, ConfigParser & parser) try {
    return new ProductIdPlugin(data, parser);
} catch (...) {
    return nullptr;
}

/// Delete the plugin instance.
void libdnf_plugin_delete_instance(plugin::IPlugin * plugin_object) {
    delete plugin_object;
}
