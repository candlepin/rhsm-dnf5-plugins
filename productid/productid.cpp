#include <libdnf5/base/base.hpp>
#include <libdnf5/conf/const.hpp>
#include <libdnf5/plugin/iplugin.hpp>

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
    /// This is utilized in informative and debugging log messages.
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

    /// Initialization method called after the Base object is created and before command-line arguments are parsed.
    /// Optional to override.
    void init() override {}

    /// Cleanup method called when plugin objects are garbage collected.
    /// Optional to override.
    void finish() noexcept override {}

    /// Override the hooks you want to implement.
    void post_base_setup() override {
    }
    void pre_transaction(const base::Transaction & transaction) override {
        pre_transaction_magic(transaction);
    };

    ConfigParser & config;

private:
    void pre_transaction_magic(const base::Transaction &) const;
};

/// Example how to implement additional logic before starting the transaction.
void ProductIdPlugin::pre_transaction_magic(const base::Transaction & transaction) const {
    (void)transaction;
    if (config.has_option("main", "enabled")) {
        Base & base = get_base();
        base.get_logger()->debug("Productid plugin enabled");
        std::cout << "Productid plugin enabled" << std::endl;
    }
}

}  // namespace

/// Below is a block of functions with C linkage used for loading the plugin binaries from disk.
/// All of these are MANDATORY to implement.

/// Return the plugin's API version.
PluginAPIVersion libdnf_plugin_get_api_version(void) {
    return REQUIRED_PLUGIN_API_VERSION;
}

/// Return the plugin's name.
const char * libdnf_plugin_get_name(void) {
    return PLUGIN_NAME;
}

/// Return plugin's version.
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
