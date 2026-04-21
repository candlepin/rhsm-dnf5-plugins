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
