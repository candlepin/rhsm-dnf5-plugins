#include <libdnf5/base/base.hpp>
#include <libdnf5/plugin/iplugin.hpp>

#include <cstring>
#include <unistd.h>

using namespace libdnf5;

namespace {
    constexpr const char *PLUGIN_NAME{"rhsm"};
    constexpr plugin::Version PLUGIN_VERSION{.major = 0, .minor = 1, .micro = 0};
    constexpr PluginAPIVersion REQUIRED_PLUGIN_API_VERSION{.major = 2, .minor = 0};

    constexpr const char *attrs[]{"author.name", "author.email", "description", nullptr};
    constexpr const char *attrs_value[]{
        "Josh Locash",
        "jlocash@redhat.com",
        "RHSM plugin for subscription status checks and warnings."
    };


    class RhsmPlugin final : public plugin::IPlugin {
    public:
        /// Implement a custom constructor for the new plugin.
        /// This is not necessary when you only need the Base object for your implementation.
        /// Optional to override.
        RhsmPlugin(plugin::IPluginData &data, ConfigParser &config) : IPlugin(data), config(config) {
        }

        /// Fill in the API version of your plugin.
        /// This is used to check if the provided plugin API version is compatible with the library's plugin API version.
        /// MANDATORY to override.
        [[nodiscard]] PluginAPIVersion get_api_version() const noexcept override { return REQUIRED_PLUGIN_API_VERSION; }

        /// Enter the name of your new plugin.
        /// This is used in log messages when an action or error related to the plugin occurs.
        /// MANDATORY to override.
        [[nodiscard]] const char *get_name() const noexcept override { return PLUGIN_NAME; }

        /// Fill in the version of your plugin.
        /// This is used in informative and debugging log messages.
        /// MANDATORY to override.
        [[nodiscard]] plugin::Version get_version() const noexcept override { return PLUGIN_VERSION; }

        /// Add custom attributes, such as information about yourself and a description of the plugin.
        /// These can be used to query plugin-specific data through the API.
        /// Optional to override.
        [[nodiscard]] const char *const *get_attributes() const noexcept override { return attrs; }

        const char *get_attribute(const char *attribute) const noexcept override {
            for (size_t i = 0; attrs[i]; ++i) {
                if (std::strcmp(attribute, attrs[i]) == 0) {
                    return attrs_value[i];
                }
            }
            return nullptr;
        }

        void post_base_setup() override { print_warnings(); };

        ConfigParser &config;

    private:
        void print_warnings() const;

        template<typename... Ss>
        void debug_log(std::string_view format, Ss &&... args) const;

        template<typename... Ss>
        void info_log(std::string_view format, Ss &&... args) const;

        template<typename... Ss>
        void warning_log(std::string_view format, Ss &&... args) const;
    };


    template<typename... Ss>
    void RhsmPlugin::debug_log(const std::string_view format, Ss &&... args) const {
        get_base().get_logger()->debug("[rhsm plugin] " + std::string(format), std::forward<Ss>(args)...);
    }

    template<typename... Ss>
    void RhsmPlugin::info_log(const std::string_view format, Ss &&... args) const {
        get_base().get_logger()->info("[rhsm plugin] " + std::string(format), std::forward<Ss>(args)...);
    }

    template<typename... Ss>
    void RhsmPlugin::warning_log(const std::string_view format, Ss &&... args) const {
        get_base().get_logger()->warning("[rhsm plugin] " + std::string(format), std::forward<Ss>(args)...);
    }

    // Print warning and info messages about subscription status.
    void RhsmPlugin::print_warnings() const {
        debug_log("Hook post_base_setup started");

        debug_log("Hook post_base_setup finished");
    }

} // namespace


/// Below is a block of functions with C linkage used for loading the plugin binaries from disk.
/// All of these are MANDATORY to implement.

/// Return plugin's API version.
PluginAPIVersion libdnf_plugin_get_api_version(void) {
    return REQUIRED_PLUGIN_API_VERSION;
}

/// Return plugin's name.
const char *libdnf_plugin_get_name(void) {
    return PLUGIN_NAME;
}

/// Return plugin's version.
plugin::Version libdnf_plugin_get_version(void) {
    return PLUGIN_VERSION;
}

/// Return the instance of the implemented plugin.
plugin::IPlugin *libdnf_plugin_new_instance(
    [[maybe_unused]] LibraryVersion library_version, plugin::IPluginData &data, ConfigParser &parser) try {
    return new RhsmPlugin(data, parser);
} catch (...) {
    return nullptr;
}

/// Delete the plugin instance.
void libdnf_plugin_delete_instance(plugin::IPlugin *plugin_object) {
    delete plugin_object;
}
