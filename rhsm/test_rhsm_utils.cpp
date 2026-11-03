#include <gtest/gtest.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

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


// --- in_container ---
// This is environment-dependent so we just verify it compiles and runs without crashing.

TEST(ContainerDetectionTest, DoesNotCrash) {
    // Just verify the function executes without error
    [[maybe_unused]] bool result = in_container();
}


// --- refresh_content tests ---
//
// refresh_content() talks to the rhc Varlink service listening
// on VARLINK_SOCKET_FILEPATH. In the unit-test environment there is no
// functioning service that the test process can reach (the socket is either
// absent, as in CI, or only accessible to root), so the VarlinkCTX constructor
// fails to connect and the function reports the error by throwing
// std::runtime_error. We cannot mock the service here, so these tests exercise
// the observable behavior in that unavailable-service environment.

class RefreshEntitlementCertificatesTest : public ::testing::Test {
protected:
    RefreshInfo refresh_info{};

    void SetUp() override {
        // Pre-set every flag to true so we can prove the function resets the
        // *_finished flags back to false before it attempts the refresh.
        refresh_info.entitlement_cert_refresh_finished = true;
        refresh_info.entitlement_cert_refresh_success = true;
        refresh_info.repo_overrides_refresh_finished = true;
        refresh_info.repo_overrides_refresh_success = true;
        refresh_info.releasever_refresh_finished = true;
        refresh_info.releasever_refresh_success = true;
    }
};

TEST_F(RefreshEntitlementCertificatesTest, NoVarlinkService_Throws) {
    if (getuid() != 0) {
        EXPECT_THROW(refresh_content(&refresh_info, false), std::runtime_error);
    }
}

TEST_F(RefreshEntitlementCertificatesTest, NoVarlinkService_ThrowsWithForce) {
    if (getuid() != 0) {
        EXPECT_THROW(refresh_content(&refresh_info, true), std::runtime_error);
    }
}

TEST_F(RefreshEntitlementCertificatesTest, ResetsFinishedFlagsBeforeConnecting) {
    // The function resets the *_finished flags to false as its first step,
    // before it tries to connect. Because the connection then fails and the
    // function throws before any reply callback runs, the flags stay false.
    try {
        refresh_content(&refresh_info, false);
        if (getuid() != 0) {
            FAIL() << "Expected refresh_content() to throw";
        }
    } catch (const std::runtime_error &) {
        // Expected in the unit-test environment.
    }

    if (getuid() != 0) {
        EXPECT_FALSE(refresh_info.entitlement_cert_refresh_finished);
        EXPECT_FALSE(refresh_info.repo_overrides_refresh_finished);
        EXPECT_FALSE(refresh_info.releasever_refresh_finished);
    }
}


int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
