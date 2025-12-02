//
// Created by jhnidek on 04.12.25.
//

#include <gtest/gtest.h>
#include <libdnf5/utils/fs/temp.hpp>

#include "productdb.hpp"
#include "utils.hpp"

class UtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

namespace test_decompress_productid_cert {
    TEST_F(UtilsTest, DecompressProductIdCertSuccess) {
        std::filesystem::path input_path = "test_data/beea371342cde7daf5b1da602a14ef545b0962c58e75f541ed31177bab5d867a-productid.gz";
        auto data = decompress_productid_cert(input_path);
        EXPECT_FALSE(data.empty());
        // TODO: compare content with 38091.pem file
    }

    TEST_F(UtilsTest, DecompressProductIdCertInvalidInput) {
        std::filesystem::path input_path = "test_data/nonexistent.pem.gz";
        EXPECT_THROW({
            try {
                auto data = decompress_productid_cert(input_path);
            } catch (const std::exception& e) {
                EXPECT_EQ(
                    e.what(),
                    std::string("cannot open file: (2) - No such file or directory [test_data/nonexistent.pem.gz]"));
                throw;
            }
        }, std::runtime_error);
    }
}

namespace test_get_product_id_from_cert_content {
    TEST_F(UtilsTest, GetProductIdFromValidProductCert) {
        std::filesystem::path input_path = "test_data/38091.pem";
        auto file = libdnf5::utils::fs::File(input_path, "rb", false);
        auto data = file.read();
        auto product_id = get_product_id_from_cert_content(data);
        EXPECT_EQ(product_id, "38091");
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
