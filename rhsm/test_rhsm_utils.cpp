#include <gtest/gtest.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "rhsm_utils.hpp"

namespace fs = std::filesystem;

using namespace std::chrono_literals;


class RhsmUtilsTest : public ::testing::Test {
protected:
    fs::path temp_dir;

    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "rhsm_test";
        fs::create_directories(temp_dir);
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }
};


// --- has_consumer_certificate tests ---

TEST_F(RhsmUtilsTest, HasConsumerCertificate_EmptyDir) {
    EXPECT_FALSE(has_consumer_certificate(temp_dir));
}

TEST_F(RhsmUtilsTest, HasConsumerCertificate_NonexistentDir) {
    EXPECT_FALSE(has_consumer_certificate(temp_dir / "nonexistent"));
}

TEST_F(RhsmUtilsTest, HasConsumerCertificate_WithPemFile) {
    std::ofstream(temp_dir / "cert.pem") << "placeholder";
    EXPECT_TRUE(has_consumer_certificate(temp_dir));
}

TEST_F(RhsmUtilsTest, HasConsumerCertificate_WithNonPemFile) {
    std::ofstream(temp_dir / "cert.txt") << "not a cert";
    EXPECT_FALSE(has_consumer_certificate(temp_dir));
}


// --- has_entitlement_certificates tests ---

TEST_F(RhsmUtilsTest, HasEntitlementCertificates_EmptyDir) {
    EXPECT_FALSE(has_entitlement_certificates(temp_dir));
}

TEST_F(RhsmUtilsTest, HasEntitlementCertificates_NonexistentDir) {
    EXPECT_FALSE(has_entitlement_certificates(temp_dir / "nonexistent"));
}

TEST_F(RhsmUtilsTest, HasEntitlementCertificates_WithPemFile) {
    std::ofstream(temp_dir / "entitlement.pem") << "placeholder";
    EXPECT_TRUE(has_entitlement_certificates(temp_dir));
}

TEST_F(RhsmUtilsTest, HasEntitlementCertificates_WithMultiplePemFiles) {
    std::ofstream(temp_dir / "1234.pem") << "cert1";
    std::ofstream(temp_dir / "5678.pem") << "cert2";
    EXPECT_TRUE(has_entitlement_certificates(temp_dir));
}


// --- is_cert_expired tests ---

class CertExpiryTest : public ::testing::Test {
protected:
    fs::path temp_dir;
    static fs::path test_data_dir;

    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "rhsm_cert_expiry_test";
        fs::create_directories(temp_dir);
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    static void SetUpTestSuite() {
        test_data_dir = fs::path("test_data");
        if (!fs::exists(test_data_dir)) {
            test_data_dir = fs::path(TEST_DATA_DIR);
        }
    }
};

fs::path CertExpiryTest::test_data_dir;

TEST_F(CertExpiryTest, ExpiredCert_ReturnsTrue) {
    EXPECT_TRUE(is_cert_expired(test_data_dir / "expired.pem"));
}

TEST_F(CertExpiryTest, ValidCert_ReturnsFalse) {
    EXPECT_FALSE(is_cert_expired(test_data_dir / "valid.pem"));
}

TEST_F(CertExpiryTest, NonexistentFile_Throws) {
    EXPECT_THROW(is_cert_expired(temp_dir / "nonexistent.pem"), std::runtime_error);
}

TEST_F(CertExpiryTest, InvalidPemContent_Throws) {
    std::ofstream(temp_dir / "corrupt.pem") << "this is not a valid certificate";
    EXPECT_THROW(is_cert_expired(temp_dir / "corrupt.pem"), std::runtime_error);
}


// --- get_releasever tests ---

TEST_F(RhsmUtilsTest, GetReleasever_NonexistentFile) {
    EXPECT_ANY_THROW(get_releasever(temp_dir / "releasever"));
}

TEST_F(RhsmUtilsTest, GetReleasever_WithContent) {
    auto filepath = temp_dir / "releasever";
    std::ofstream(filepath) << "9.4";
    auto result = get_releasever(filepath);
    EXPECT_EQ(result, "9.4");
}

TEST_F(RhsmUtilsTest, GetReleasever_WithWhitespace) {
    auto filepath = temp_dir / "releasever";
    std::ofstream(filepath) << "  9.4  \n";
    auto result = get_releasever(filepath);
    EXPECT_EQ(result, "9.4");
}

TEST_F(RhsmUtilsTest, GetReleasever_EmptyFile) {
    auto filepath = temp_dir / "releasever";
    std::ofstream(filepath) << "";
    auto result = get_releasever(filepath);
    EXPECT_EQ(result, "");
}

TEST_F(RhsmUtilsTest, GetReleasever_WhitespaceOnly) {
    auto filepath = temp_dir / "releasever";
    std::ofstream(filepath) << "   \n";
    auto result = get_releasever(filepath);
    EXPECT_EQ(result, "");
}

TEST_F(RhsmUtilsTest, GetReleasever_OnlyFirstLine) {
    auto filepath = temp_dir / "releasever";
    std::ofstream(filepath) << "9.4\n10.0\n";
    auto result = get_releasever(filepath);
    EXPECT_EQ(result, "9.4");
}


// --- generate_uuid tests ---

TEST(RhsmUtilsUuidTest, UuidHasValidFormat) {
    const std::string uuid = generate_uuid();

    // The canonical representation has 36 characters: 8-4-4-4-12
    ASSERT_EQ(uuid.length(), 36u);
    EXPECT_EQ(uuid[8], '-');
    EXPECT_EQ(uuid[13], '-');
    EXPECT_EQ(uuid[18], '-');
    EXPECT_EQ(uuid[23], '-');

    // All remaining characters have to be lowercase hexadecimal digits
    for (const char c : uuid) {
        if (c == '-') {
            continue;
        }
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            << "Unexpected character '" << c << "' in UUID";
    }
}

TEST(RhsmUtilsUuidTest, UuidIsVersion4) {
    const std::string uuid = generate_uuid();

    // The version nibble of an RFC 4122 version 4 UUID has to be 4
    EXPECT_EQ(uuid[14], '4');

    // The variant nibble of the RFC 4122 variant has to be one of 8, 9, a, b
    const char variant = uuid[19];
    EXPECT_TRUE(variant == '8' || variant == '9' || variant == 'a' || variant == 'b')
        << "Unexpected variant nibble '" << variant << "' in UUID";
}

TEST(RhsmUtilsUuidTest, UuidsAreUnique) {
    std::set<std::string> uuids;
    for (int i = 0; i < 1000; ++i) {
        uuids.insert(generate_uuid());
    }
    EXPECT_EQ(uuids.size(), 1000u);
}


// --- get_locale tests ---

namespace {
    /// Save the current value of an environment variable and restore it (or
    /// remove it, when it was not set) at the end of the test.
    class EnvVarGuard {
    public:
        explicit EnvVarGuard(const char *name) : name(name) {
            if (const char *const value = std::getenv(name); value != nullptr) {
                original_value = value;
                was_set = true;
            }
        }

        ~EnvVarGuard() {
            if (was_set) {
                setenv(name, original_value.c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

    private:
        const char *name;
        std::string original_value;
        bool was_set = false;
    };
} // namespace

class RhsmLocaleTest : public ::testing::Test {
protected:
    EnvVarGuard lc_all{"LC_ALL"};
    EnvVarGuard lc_messages{"LC_MESSAGES"};
    EnvVarGuard lang{"LANG"};

    void SetUp() override {
        // Start each test with a clean environment. The guards above restore
        // the original values, when the test fixture is destroyed.
        unsetenv("LC_ALL");
        unsetenv("LC_MESSAGES");
        unsetenv("LANG");
    }
};

TEST_F(RhsmLocaleTest, NoLocaleVariablesSet) {
    EXPECT_EQ(get_locale(), "C.UTF-8");
}

TEST_F(RhsmLocaleTest, LcAllHasHighestPrecedence) {
    setenv("LC_ALL", "en_US.UTF-8", 1);
    setenv("LC_MESSAGES", "fr_FR.UTF-8", 1);
    setenv("LANG", "de_DE.UTF-8", 1);
    EXPECT_EQ(get_locale(), "en_US.UTF-8");
}

TEST_F(RhsmLocaleTest, LcMessagesPrecedesLang) {
    setenv("LC_MESSAGES", "fr_FR.UTF-8", 1);
    setenv("LANG", "de_DE.UTF-8", 1);
    EXPECT_EQ(get_locale(), "fr_FR.UTF-8");
}

TEST_F(RhsmLocaleTest, LangUsedWhenOthersUnset) {
    setenv("LANG", "de_DE.UTF-8", 1);
    EXPECT_EQ(get_locale(), "de_DE.UTF-8");
}

TEST_F(RhsmLocaleTest, EmptyLcAllIsIgnored) {
    setenv("LC_ALL", "", 1);
    setenv("LANG", "de_DE.UTF-8", 1);
    EXPECT_EQ(get_locale(), "de_DE.UTF-8");
}

TEST_F(RhsmLocaleTest, AllEmptyValuesAreIgnored) {
    setenv("LC_ALL", "", 1);
    setenv("LC_MESSAGES", "", 1);
    setenv("LANG", "", 1);
    EXPECT_EQ(get_locale(), "C.UTF-8");
}


// --- is_repo_file_outdated tests ---

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_NonexistentCertDir) {
    EXPECT_FALSE(is_repo_file_outdated(temp_dir / "nonexistent", temp_dir / "redhat.repo"));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_CertDirIsFile) {
    const auto not_a_dir = temp_dir / "not_a_dir";
    std::ofstream(not_a_dir) << "placeholder";
    EXPECT_FALSE(is_repo_file_outdated(not_a_dir, temp_dir / "redhat.repo"));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_EmptyCertDir) {
    EXPECT_FALSE(is_repo_file_outdated(temp_dir, temp_dir / "redhat.repo"));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_NonPemFilesIgnored) {
    std::ofstream(temp_dir / "notes.txt") << "not a certificate";
    EXPECT_FALSE(is_repo_file_outdated(temp_dir, temp_dir / "redhat.repo"));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_KeyFilesOnly) {
    std::ofstream(temp_dir / "1234567890-key.pem") << "key";
    EXPECT_FALSE(is_repo_file_outdated(temp_dir, temp_dir / "redhat.repo"));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_MissingRepoFile) {
    std::ofstream(temp_dir / "1234567890.pem") << "certificate";
    EXPECT_TRUE(is_repo_file_outdated(temp_dir, temp_dir / "redhat.repo"));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_CertNewerThanRepoFile) {
    const auto repo_file = temp_dir / "redhat.repo";
    std::ofstream(repo_file) << "[repo]";
    std::ofstream(temp_dir / "1234567890.pem") << "certificate";

    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(repo_file, now - 2h);
    fs::last_write_time(temp_dir / "1234567890.pem", now - 1h);
    EXPECT_TRUE(is_repo_file_outdated(temp_dir, repo_file));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_CertOlderThanRepoFile) {
    const auto repo_file = temp_dir / "redhat.repo";
    std::ofstream(repo_file) << "[repo]";
    std::ofstream(temp_dir / "1234567890.pem") << "certificate";

    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(repo_file, now - 1h);
    fs::last_write_time(temp_dir / "1234567890.pem", now - 2h);
    EXPECT_FALSE(is_repo_file_outdated(temp_dir, repo_file));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_SameModificationTime) {
    const auto repo_file = temp_dir / "redhat.repo";
    std::ofstream(repo_file) << "[repo]";
    std::ofstream(temp_dir / "1234567890.pem") << "certificate";

    const auto time = fs::file_time_type::clock::now() - 1h;
    fs::last_write_time(repo_file, time);
    fs::last_write_time(temp_dir / "1234567890.pem", time);
    EXPECT_FALSE(is_repo_file_outdated(temp_dir, repo_file));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_NewestCertDecides) {
    const auto repo_file = temp_dir / "redhat.repo";
    std::ofstream(repo_file) << "[repo]";
    std::ofstream(temp_dir / "1111111111.pem") << "certificate1";
    std::ofstream(temp_dir / "2222222222.pem") << "certificate2";

    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(repo_file, now - 4h);
    fs::last_write_time(temp_dir / "1111111111.pem", now - 3h);
    fs::last_write_time(temp_dir / "2222222222.pem", now - 2h);
    EXPECT_TRUE(is_repo_file_outdated(temp_dir, repo_file));
}

TEST_F(RhsmUtilsTest, IsRepoFileOutdated_KeyFilesIgnored) {
    const auto repo_file = temp_dir / "redhat.repo";
    std::ofstream(repo_file) << "[repo]";
    std::ofstream(temp_dir / "1234567890.pem") << "certificate";
    std::ofstream(temp_dir / "1234567890-key.pem") << "key";

    // The key file is newer than the repo file, but key files have to be
    // ignored.
    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(repo_file, now - 2h);
    fs::last_write_time(temp_dir / "1234567890.pem", now - 3h);
    fs::last_write_time(temp_dir / "1234567890-key.pem", now - 1h);
    EXPECT_FALSE(is_repo_file_outdated(temp_dir, repo_file));
}


// --- is_full_refresh_enabled tests ---

TEST(FullRefreshOptionTest, AcceptedTruthyValues) {
    EXPECT_TRUE(is_full_refresh_enabled("true"));
    EXPECT_TRUE(is_full_refresh_enabled("yes"));
    EXPECT_TRUE(is_full_refresh_enabled("1"));
}

TEST(FullRefreshOptionTest, CaseInsensitive) {
    EXPECT_TRUE(is_full_refresh_enabled("TRUE"));
    EXPECT_TRUE(is_full_refresh_enabled("True"));
    EXPECT_TRUE(is_full_refresh_enabled("YeS"));
}

TEST(FullRefreshOptionTest, FalsyValues) {
    EXPECT_FALSE(is_full_refresh_enabled("false"));
    EXPECT_FALSE(is_full_refresh_enabled("no"));
    EXPECT_FALSE(is_full_refresh_enabled("0"));
}

TEST(FullRefreshOptionTest, EmptyAndUnknownValues) {
    EXPECT_FALSE(is_full_refresh_enabled(""));
    EXPECT_FALSE(is_full_refresh_enabled("  "));
    EXPECT_FALSE(is_full_refresh_enabled("enabled"));
    // Leading/trailing whitespace is not trimmed, so "1 " is not truthy.
    EXPECT_FALSE(is_full_refresh_enabled("1 "));
}


// --- should_refresh_content tests ---

TEST(ShouldRefreshContentTest, NoReasonDoesNotRefresh) {
    EXPECT_FALSE(should_refresh_content(false, false, false));
}

TEST(ShouldRefreshContentTest, ExpiredCertificateTriggersRefresh) {
    EXPECT_TRUE(should_refresh_content(true, false, false));
}

TEST(ShouldRefreshContentTest, FullRefreshTriggersRefresh) {
    EXPECT_TRUE(should_refresh_content(false, true, false));
}

TEST(ShouldRefreshContentTest, OutdatedRepoFileTriggersRefresh) {
    EXPECT_TRUE(should_refresh_content(false, false, true));
}

TEST(ShouldRefreshContentTest, MultipleReasonsTriggerRefresh) {
    EXPECT_TRUE(should_refresh_content(true, true, true));
}


// --- in_container ---
// This is environment-dependent so we just verify it compiles and runs without crashing.

TEST(ContainerDetectionTest, DoesNotCrash) {
    // Just verify the function executes without error
    [[maybe_unused]] bool result = in_container();
}


// --- refresh_content tests ---
//
// refresh_content() talks to the rhc Varlink service listening on
// VARLINK_SOCKET_FILEPATH. When it connects successfully it performs a *real*
// content refresh: it refreshes entitlement certificates, rewrites redhat.repo,
// sets the release version and writes repository overrides. The unit tests must
// never trigger that against a live service, and they can only assert the
// error-handling path when the service is absent.
//
// Whether a refresh would really happen depends on whether the socket is
// actually connectable, not on the user id: when the tests run as root (as they
// do in CI, where they execute inside a container) a running service would be
// reachable. We therefore probe the socket directly and skip the tests when it
// is reachable, instead of silently asserting nothing based on getuid().

namespace {
    /// Return true when the rhc Varlink socket can be connected to.
    ///
    /// The probe only opens and immediately closes the connection; it never
    /// sends a Varlink method call, so it has no effect on a running service.
    bool varlink_socket_is_reachable() {
        std::string_view endpoint = VARLINK_SOCKET_FILEPATH;
        constexpr std::string_view unix_prefix = "unix:";
        if (endpoint.starts_with(unix_prefix)) {
            endpoint.remove_prefix(unix_prefix.size());
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        // The path has to fit into sun_path together with its NUL terminator.
        if (endpoint.size() >= sizeof(addr.sun_path)) {
            return false;
        }
        std::memcpy(addr.sun_path, endpoint.data(), endpoint.size());

        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            return false;
        }

        const bool reachable =
            ::connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == 0;
        ::close(fd);
        return reachable;
    }
}

class RefreshEntitlementCertificatesTest : public ::testing::Test {
protected:
    RefreshInfo refresh_info{};

    void SetUp() override {
        // Pre-set every *_finished flag to true so we can prove the function
        // resets them back to false before it attempts the refresh.
        refresh_info.entitlement_cert_refresh_finished = true;
        refresh_info.repo_overrides_refresh_finished = true;
        refresh_info.releasever_refresh_finished = true;
    }
};

TEST_F(RefreshEntitlementCertificatesTest, NoVarlinkService_Throws) {
    if (varlink_socket_is_reachable()) {
        GTEST_SKIP() << "rhc Varlink service is reachable; skipping to avoid a real content refresh";
    }
    EXPECT_THROW(refresh_content(&refresh_info, false), std::runtime_error);
}

TEST_F(RefreshEntitlementCertificatesTest, NoVarlinkService_ThrowsWithForce) {
    if (varlink_socket_is_reachable()) {
        GTEST_SKIP() << "rhc Varlink service is reachable; skipping to avoid a real content refresh";
    }
    EXPECT_THROW(refresh_content(&refresh_info, true), std::runtime_error);
}

TEST_F(RefreshEntitlementCertificatesTest, ResetsFinishedFlagsBeforeConnecting) {
    if (varlink_socket_is_reachable()) {
        GTEST_SKIP() << "rhc Varlink service is reachable; skipping to avoid a real content refresh";
    }

    // The function resets the *_finished flags to false as its first step,
    // before it tries to connect. Because the connection then fails and the
    // function throws before any reply callback runs, the flags stay false.
    EXPECT_THROW(refresh_content(&refresh_info, false), std::runtime_error);

    EXPECT_FALSE(refresh_info.entitlement_cert_refresh_finished);
    EXPECT_FALSE(refresh_info.repo_overrides_refresh_finished);
    EXPECT_FALSE(refresh_info.releasever_refresh_finished);
}


int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
