#include <libdnf5/base/base.hpp>
#include <libdnf5/plugin/iplugin.hpp>

#include <cstring>
#include <format>
#include <iostream>
#include <unistd.h>

#include "rhsm_utils.hpp"

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

        void warn_system_not_registered() const;

        void warn_no_entitlements() const;

        void warn_entitlements_expired() const;

        void log_releasever() const;

        template<typename... Ss>
        void debug_log(std::string_view format, Ss &&... args) const;

        template<typename... Ss>
        void info_log(std::string_view format, Ss &&... args) const;

        template<typename... Ss>
        void warning_log(std::string_view format, Ss &&... args) const;

        template<typename... Ss>
        void error_log(std::string_view format, Ss &&... args) const;
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

    template<typename... Ss>
    void RhsmPlugin::error_log(const std::string_view format, Ss &&... args) const {
        get_base().get_logger()->error("[rhsm plugin] " + std::string(format), std::forward<Ss>(args)...);
    }

    // Print warning and info messages about subscription status.
    void RhsmPlugin::print_warnings() const {
        debug_log("Hook post_base_setup started");

        if (getuid() != 0) {
            info_log("Not root, Subscription Management repositories not updated");

            // FIXME: replace with appropriate DNF API call when available
            std::cout << "Not root, Subscription Management repositories not updated" << std::endl;
            return;
        }

        if (!in_container()) {
            warn_system_not_registered();
            warn_no_entitlements();
        }

        warn_entitlements_expired();
        log_releasever();

        debug_log("Hook post_base_setup finished");
    }

    // Log a warning message when the system is not registered (consumer certificate does not exist in /etc/pki/consumer)
    void RhsmPlugin::warn_system_not_registered() const {
        if (!has_consumer_certificate(CONSUMER_CERT_DIR)) {
            warning_log("System is not registered. No consumer certificate found in {}.", CONSUMER_CERT_DIR);

            // FIXME: replace with appropriate DNF API call when available
            std::cout << std::format("System is not registered. No consumer certificate found in {}.",
                                     CONSUMER_CERT_DIR) << std::endl;
        }
    }

    // Log a warning message when no entitlement certificate exists in /etc/pki/entitlement
    void RhsmPlugin::warn_no_entitlements() const {
        if (!has_entitlement_certificates(ENTITLEMENT_CERT_DIR)) {
            warning_log("No SCA entitlement certificate(s) found. {} directory is empty.", ENTITLEMENT_CERT_DIR);

            // FIXME: replace with appropriate DNF API call when available
            std::cout << std::format("No SCA entitlement certificate(s) found. {} directory is empty.",
                                     ENTITLEMENT_CERT_DIR) << std::endl;
        }
    }

    // Log a warning message when SCA entitlement certificate(s) are expired
    void RhsmPlugin::warn_entitlements_expired() const {
        auto [expired, unreadable] = get_expired_entitlements(ENTITLEMENT_CERT_DIR);

        for (const auto &path : unreadable) {
            warning_log("Failed to read or parse entitlement certificate: {}", path.string());
        }

        if (expired.empty()) {
            return;
        }

        std::string expired_list;
        for (const auto &entitlement: expired) {
            expired_list += "  - " + entitlement + "\n";
        }

        error_log(
            "The following entitlement certificate(s) have expired:\n{}"
            "Renew your subscription to resume access to updates.",
            expired_list);

        // FIXME: replace with appropriate DNF API call when available
        std::cerr << std::format(
            "The following entitlement certificate(s) have expired:\n{}"
            "Renew your subscription to resume access to updates.",
            expired_list) << std::endl;
    }

    // Checks for the presence of /etc/dnf/var/releasever; if exists, then logs its value in an info message
    void RhsmPlugin::log_releasever() const {
        try {
            // Release version check
            auto releasever = get_releasever(RELEASEVER_FILE);
            if (!releasever.empty()) {
                info_log(
                    "This system has release set to {} and it receives updates only for this release.",
                    releasever);

                // FIXME: replace with appropriate DNF API call when available
                std::cout << std::format(
                    "This system has release set to {} and it receives updates only for this release.",
                    releasever) << std::endl;
            }
        } catch (const std::exception &e) {
            warning_log("Unable to determine release version: {}", e.what());
        }
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
