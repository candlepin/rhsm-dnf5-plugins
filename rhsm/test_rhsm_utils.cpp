#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "rhsm_utils.hpp"

namespace fs = std::filesystem;


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


// --- get_expired_entitlements tests ---

class EntitlementExpiryTest : public ::testing::Test {
protected:
    fs::path temp_dir;
    static fs::path test_data_dir;

    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "rhsm_expiry_test";
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

fs::path EntitlementExpiryTest::test_data_dir;

TEST_F(EntitlementExpiryTest, GetExpired_EmptyDir) {
    auto result = get_expired_entitlements(temp_dir);
    EXPECT_TRUE(result.empty());
}

TEST_F(EntitlementExpiryTest, GetExpired_NonexistentDir) {
    auto result = get_expired_entitlements(temp_dir / "nonexistent");
    EXPECT_TRUE(result.empty());
}

TEST_F(EntitlementExpiryTest, GetExpired_SingleExpiredCert) {
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "1234.pem");
    auto result = get_expired_entitlements(temp_dir);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "1234");
}

TEST_F(EntitlementExpiryTest, GetExpired_SingleValidCert) {
    fs::copy_file(test_data_dir / "valid.pem", temp_dir / "5678.pem");
    auto result = get_expired_entitlements(temp_dir);
    EXPECT_TRUE(result.empty());
}

TEST_F(EntitlementExpiryTest, GetExpired_MixedCerts_OnlyExpiredReturned) {
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "1234.pem");
    fs::copy_file(test_data_dir / "valid.pem", temp_dir / "5678.pem");
    auto result = get_expired_entitlements(temp_dir);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "1234");
}

TEST_F(EntitlementExpiryTest, GetExpired_KeyFilesSkipped) {
    // An expired cert named as a key file should not appear in results
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "1234-key.pem");
    auto result = get_expired_entitlements(temp_dir);
    EXPECT_TRUE(result.empty());
}

TEST_F(EntitlementExpiryTest, GetExpired_NonPemFilesIgnored) {
    // Non-pem files should be completely ignored, even if they contain cert data
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "1234.txt");
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "cert.der");
    std::ofstream(temp_dir / "notes.conf") << "some config";
    auto result = get_expired_entitlements(temp_dir);
    EXPECT_TRUE(result.empty());
}

TEST_F(EntitlementExpiryTest, GetExpired_MultipleExpired_ReturnsSorted) {
    // Use the same expired cert with different names to verify sorted output
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "zebra.pem");
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "alpha.pem");
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "middle.pem");
    auto result = get_expired_entitlements(temp_dir);
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "alpha");
    EXPECT_EQ(result[1], "middle");
    EXPECT_EQ(result[2], "zebra");
}

TEST_F(EntitlementExpiryTest, GetExpired_InvalidPemContent) {
    // A .pem file with garbage content should be silently skipped
    std::ofstream(temp_dir / "corrupt.pem") << "this is not a valid certificate";
    auto result = get_expired_entitlements(temp_dir);
    EXPECT_TRUE(result.empty());
}

TEST_F(EntitlementExpiryTest, GetExpired_MixedExpiredValidAndKey) {
    // Comprehensive scenario: expired certs, valid certs, key files, non-pem files
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "productA.pem");
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "productA-key.pem");
    fs::copy_file(test_data_dir / "expired.pem", temp_dir / "productB.pem");
    fs::copy_file(test_data_dir / "valid.pem", temp_dir / "productC.pem");
    fs::copy_file(test_data_dir / "valid.pem", temp_dir / "productC-key.pem");
    std::ofstream(temp_dir / "readme.txt") << "ignore me";
    auto result = get_expired_entitlements(temp_dir);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "productA");
    EXPECT_EQ(result[1], "productB");
}

TEST_F(EntitlementExpiryTest, GetExpired_RegularFileNotDir) {
    // Passing a regular file (not a directory) should return empty
    auto filepath = temp_dir / "afile.pem";
    fs::copy_file(test_data_dir / "expired.pem", filepath);
    auto result = get_expired_entitlements(filepath);
    EXPECT_TRUE(result.empty());
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


// --- in_container ---
// This is environment-dependent so we just verify it compiles and runs without crashing.

TEST(ContainerDetectionTest, DoesNotCrash) {
    // Just verify the function executes without error
    [[maybe_unused]] bool result = in_container();
}


int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
