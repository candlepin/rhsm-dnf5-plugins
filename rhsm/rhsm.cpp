#include <libdnf5/base/base.hpp>
#include <libdnf5/plugin/iplugin.hpp>
#include <libdnf5/utils/bgettext/bgettext-mark-domain.h>
#include <libdnf5/utils/format_locale.hpp>

#include <cstring>
#include <format>
#include <iostream>
#include <unistd.h>
#include <openssl/pem.h>
#include <openssl/types.h>
#include <openssl/x509.h>

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

    /// Class for displaying simple messages without any argument.
    class RhsmSimpleMessage : public Message {
    public:
        explicit RhsmSimpleMessage(const BgettextMessage text) : text(text) {}

        std::string format(const bool translate, const utils::Locale * locale) const override {
            return utils::format(locale, translate, text, 1);
        }

    private:
        BgettextMessage text;
    };

    /// Class for displaying message with no installed entitlement certificate.
    class RhsmNoEntitlementCertMessage : public Message {
    public:
        explicit RhsmNoEntitlementCertMessage(std::string cert_path) : cert_path(std::move(cert_path)) {}

        std::string format(const bool translate, const utils::Locale * locale) const override {
            return utils::format(
                locale,
                translate,
                M_("No SCA entitlement certificate(s) found in {}"),
                1,
                cert_path);
        }

    private:
        std::string cert_path;
    };

    /// Class for expired entitlement certificates message.
    class RhsmExpiredEntCertsMessage : public Message {
    public:
        explicit RhsmExpiredEntCertsMessage(std::vector<std::string> expired) : expired(std::move(expired)) {}

        std::string format(const bool translate, const utils::Locale * locale) const override {
            std::string expired_list;
            for (const auto &entitlement: expired) {
                expired_list += "  - " + entitlement + "\n";
            }
            return utils::format(
                locale,
                translate,
                MP_(
                    "The following entitlement certificate has expired:\n{}"
                    "Renew your subscription to resume access to updates.",
                    "The following entitlement certificates have expired:\n{}"
                    "Renew your subscription to resume access to updates."),
            expired.size(),
            expired_list
            );
        }

    private:
        std::vector<std::string> expired;
    };

    /// Class for displaying message that system has release set to a specific version.
    class RhsmReleaseVerMessage : public Message {
    public:
        explicit RhsmReleaseVerMessage(std::string release_ver) : release_ver(std::move(release_ver)) {}

        std::string format(const bool translate, const utils::Locale * locale) const override {
            return utils::format(
                locale,
                translate,
                M_("This system has release set to {} and it receives updates only for this release."),
                1,
                release_ver);
        }

    private:
        std::string release_ver;
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

        std::vector<std::string> get_expired_entitlements(const std::filesystem::path & entitlement_cert_dir) const;

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
            get_base().message(
                base::InteractionCallbacks::MessageLevel::INFO,
                RhsmSimpleMessage(M_("Not root, Subscription Management repositories not updated"))
                );
            return;
        }

        if (!in_container()) {
            const auto registered = has_consumer_certificate(CONSUMER_CERT_DIR);
            if (!registered) {
                warn_system_not_registered();
            }
            if (registered) {
                // Try to warn about missing entitlements only in situation, when system is registered
                if (!has_entitlement_certificates(ENTITLEMENT_CERT_DIR)) {
                    warn_no_entitlements();
                }
            }
        } else {
            info_log("Running in container mode. Subscription management is handled by the host.");
            get_base().message(
                base::InteractionCallbacks::MessageLevel::INFO,
                RhsmSimpleMessage(M_("This system is running in container mode. Subscription management is handled by the host."))
                );
        }

        warn_entitlements_expired();
        log_releasever();

        debug_log("Hook post_base_setup finished");
    }

    // Log a warning message when the system is not registered (consumer certificate does not exist in /etc/pki/consumer)
    void RhsmPlugin::warn_system_not_registered() const {
        warning_log("System is not registered. No consumer certificate found in {}.", CONSUMER_CERT_DIR);
        get_base().message(
            base::InteractionCallbacks::MessageLevel::WARNING,
            RhsmSimpleMessage(M_("This system is not registered with an entitlement server."
                " You can use \"rhc\" or \"subscription-manager\" to register."))
            );
    }

    // Log a warning message when no entitlement certificate exists in /etc/pki/entitlement
    void RhsmPlugin::warn_no_entitlements() const {
        warning_log("No SCA entitlement certificate(s) found in {}", ENTITLEMENT_CERT_DIR);
        get_base().message(
            base::InteractionCallbacks::MessageLevel::WARNING,
            RhsmNoEntitlementCertMessage( ENTITLEMENT_CERT_DIR)
        );
    }

    /// Scans the directory for .pem files (skipping key files), checks notAfter dates,
    /// and returns expired certificate stems.
    std::vector<std::string> RhsmPlugin::get_expired_entitlements(const std::filesystem::path & entitlement_cert_dir) const {
        namespace fs = std::filesystem;

        std::set<std::string> expired_names;

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

            try {
                if (is_cert_expired(entry.path())) {
                    expired_names.insert(stem);
                }
            } catch (std::runtime_error &e) {
                warning_log(e.what());
            }
        }

        return {expired_names.begin(), expired_names.end()};
    }

    // Log a warning message when SCA entitlement certificate(s) are expired
    void RhsmPlugin::warn_entitlements_expired() const {
        const auto expired = get_expired_entitlements(ENTITLEMENT_CERT_DIR);

        if (expired.empty()) {
            return;
        }

        std::string expired_list;
        for (const auto &entitlement: expired) {
            expired_list += entitlement + ", ";
        }

        error_log("The following entitlement certificate(s) have expired: {}",expired_list);
        get_base().message(
            base::InteractionCallbacks::MessageLevel::ERROR,
            RhsmExpiredEntCertsMessage(expired)
        );
    }

    // Checks for the presence of /etc/dnf/var/releasever; if exists, then logs its value in an info message
    void RhsmPlugin::log_releasever() const {
        try {
            // Release version check
            auto releasever = get_releasever(RELEASEVER_FILE);
            if (!releasever.empty()) {
                info_log(
                    "This system has release set to {} in {} and it receives updates only for this release.",
                    RELEASEVER_FILE, releasever);

                get_base().message(
                    base::InteractionCallbacks::MessageLevel::INFO,
                    RhsmReleaseVerMessage(releasever)
                    );
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
