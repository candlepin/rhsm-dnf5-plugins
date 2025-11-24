#include <libdnf5/base/base.hpp>
#include <libdnf5/conf/const.hpp>
#include <libdnf5/plugin/iplugin.hpp>

#include <filesystem>
#include <iostream>

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

#define PRODUCTDB_DIR "/var/lib/rhsm/"

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
    // Own logging
    template <typename... Ss>
    void debug_log(std::string_view format, Ss &&... args) const;

    template <typename... Ss>
    void warning_log(std::string_view format, Ss &&... args) const;

    template <typename... Ss>
    void error_log(std::string_view format, Ss &&... args) const;

    // Hooks
    void repos_configured_hook() const;
    void post_transaction_hook(const base::Transaction &) const;

    [[nodiscard]] bool setup_filesystem() const ;
};

template <typename... Ss>
void ProductIdPlugin::debug_log(std::string_view format, Ss &&... args) const {
    Base & base = get_base();
    base.get_logger()->debug("[productid plugin] " + libdnf5::utils::sformat(format, std::forward<Ss>(args)...));
}

template <typename... Ss>
void ProductIdPlugin::warning_log(std::string_view format, Ss &&... args) const {
    Base & base = get_base();
    base.get_logger()->warning("[productid plugin] " + libdnf5::utils::sformat(format, std::forward<Ss>(args)...));
}

template <typename... Ss>
void ProductIdPlugin::error_log(std::string_view format, Ss &&... args) const {
    Base & base = get_base();
    base.get_logger()->error("[productid plugin] " + libdnf5::utils::sformat(format, std::forward<Ss>(args)...));
}

/// This method is called before transaction processing starts, and we do here final
/// configuration adjustments. We order the dnf to also try to download productid metadata.
void ProductIdPlugin::repos_configured_hook() const {
    Base & base = get_base();
    debug_log("repos_configured hook started");
    debug_log("Order dnf to download additional metadata type: productid");
    base.get_config().get_optional_metadata_types_option().set(METADATA_TYPE_PRODUCTID);
    debug_log("repos_configured hook finished");
}
    
/// The management of productid certificates is triggered here after the transaction is finished.
/// It covers two use cases:
///
/// 1. Try to get downloaded product certificates when the new package is installed, and the
///    product certificate has not been installed yet. Install this product certificate
///    to /etc/pki/product if needed. Add the RPM repository to our "database" in
///    /var/lib/rhsm/productid.json file
/// 2. When the RPM package was removed, then if there is any repository in the
///    /var/lib/rhsm/productid.json file, then remove it from the "database". When
///    the productid database contains productid with no RPM repository, then
///    remove this productid from the database and then remove the productid certificate
///    from /etc/pki/product directory.
void ProductIdPlugin::post_transaction_hook(const base::Transaction & transaction) const {
    (void)transaction;
    Base & base = get_base();

    debug_log("post_transaction hook started");

    // First, try to create all necessary directories
    if (!setup_filesystem()) return;

    // Get the list of enabled repositories
    repo::RepoQuery repos(base);
    repos.filter_enabled(true);

    // When there are no enabled repositories, then we cannot try to get
    // any product certificate from anywhere, and we can finish this hook here.
    if (repos.empty()) {
        warning_log("No enabled repositories found; Exiting.");
        return;
    }

    // Go through all enabled repositories and try to get paths of downloaded productid certificates.
    // Note: when the transaction is e.g. "remove", then cached metadata is empty, but we will probably
    //       not need cached metadata during removal of packages.
    for (const auto & repo : repos) {
        std::string productid_path = repo->get_metadata_path(METADATA_TYPE_PRODUCTID);
        if (!productid_path.empty()) {
            debug_log(
                "productid certificates of '{}' repository downloaded to: {}",
                repo->get_id(),
                productid_path
                );
        } else {
            debug_log(
                "'{}' does not contain productid certificates",
                repo->get_id()
                );
        }
        // TODO: When productid_path is not empty, then try to install it to the system, when at least
        //       one RPM package is installed from this repository
    }

    // TODO: Try to remove installed productid certificates when needed

    debug_log("post_transaction hook finished");
}

/// This plugin needs the existence of several directories. Try to create these directories.
bool ProductIdPlugin::setup_filesystem() const {
    // Try to create the directory where we store product certificates "database"
    if (std::filesystem::exists(PRODUCTDB_DIR)) {
        debug_log("Directory {} already exists", PRODUCTDB_DIR);
    } else {
        debug_log("Directory {} does not exist, creating it", PRODUCTDB_DIR);
        try {
            std::filesystem::create_directories(PRODUCTDB_DIR);
        } catch (const std::filesystem::filesystem_error &e) {
            error_log(
                "Failed to create directory {}: {}; Exiting", PRODUCTDB_DIR, e.what());
            return false;
        }
    }
    // TODO: Create other directories when needed
    return true;
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
