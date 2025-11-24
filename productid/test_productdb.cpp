//
// Created by jhnidek on 24.11.25.
//

#include <gtest/gtest.h>
#include <fstream>
#include "productdb.hpp"

class ProductDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db.path = "test_product.json";
    }

    void TearDown() override {
        std::remove(test_db.path.c_str());
    }

    ProductDb test_db;
};

namespace  test_constructors {
    TEST_F(ProductDbTest, ConstructorWithouArguments) {
        const auto *db = new ProductDb();
        EXPECT_EQ(db->path, DEFAULT_PRODUCTDB_FILE);
        EXPECT_TRUE(db->repo_map.empty());
        delete db;
    }

    TEST_F(ProductDbTest, ConstructorWithPathArgument) {
        const auto *db = new ProductDb("foo_product.json");
        EXPECT_EQ(db->path, "foo_product.json");
        EXPECT_TRUE(db->repo_map.empty());
        delete db;
    }
}

namespace test_read_product_db {
    TEST_F(ProductDbTest, ReadEmptyDbPath) {
        test_db.path = "";
        EXPECT_FALSE(test_db.read_product_db());
        EXPECT_TRUE(test_db.repo_map.empty());
    }

    TEST_F(ProductDbTest, ReadWrongDbPath) {
        test_db.path = "./nonexistent_file.json";
        const auto ret = test_db.read_product_db();
        EXPECT_FALSE(ret);
        const auto err = strerror(errno);
        EXPECT_NE(err, nullptr);
        EXPECT_EQ(err, std::string("No such file or directory"));
        EXPECT_TRUE(test_db.repo_map.empty());
    }

    TEST_F(ProductDbTest, ReadEmptyDb) {
        EXPECT_FALSE(test_db.read_product_db());
        EXPECT_TRUE(test_db.repo_map.empty());
    }

    TEST_F(ProductDbTest, ReadValidDb) {
        std::ofstream file(test_db.path);
        file << R"({"69": ["repo1", "repo2"], "42": ["repo3"]})";
        file.close();

        EXPECT_TRUE(test_db.read_product_db());
        EXPECT_EQ(test_db.repo_map.size(), 2);
        EXPECT_TRUE(test_db.has_product_id("69"));
        EXPECT_TRUE(test_db.has_product_id("42"));
        EXPECT_TRUE(test_db.has_repo_id("69", "repo1"));
        EXPECT_TRUE(test_db.has_repo_id("69", "repo2"));
        EXPECT_TRUE(test_db.has_repo_id("42", "repo3"));
    }

    TEST_F(ProductDbTest, ReadInvalidJson) {
        std::ofstream file(test_db.path);
        file << "invalid json";
        file.close();

        EXPECT_FALSE(test_db.read_product_db());
        EXPECT_TRUE(test_db.repo_map.empty());
    }
}

namespace test_write_product_db {
    TEST_F(ProductDbTest, WriteDbToEmptyPath) {
        test_db.path = "";
        EXPECT_FALSE(test_db.write_product_db());
    }

    TEST_F(ProductDbTest, WriteDbToNonExistentPath) {
        test_db.path = "/nonexistent/path/to/file.json";
        EXPECT_FALSE(test_db.write_product_db());
        const auto err = strerror(errno);
        EXPECT_NE(err, nullptr);
        EXPECT_EQ(err, std::string("No such file or directory"));
    }
    TEST_F(ProductDbTest, WriteEmptyDb) {
        EXPECT_TRUE(test_db.write_product_db());
        std::ifstream file(test_db.path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        EXPECT_EQ(content, "{}");
    }

    TEST_F(ProductDbTest, WriteValidDb) {
        test_db.add_repo_id("69", "repo1");
        test_db.add_repo_id("69", "repo2");
        test_db.add_repo_id("42", "repo3");

        EXPECT_TRUE(test_db.write_product_db());

        std::ifstream file(test_db.path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        EXPECT_TRUE(content.find("\"69\"") != std::string::npos);
        EXPECT_TRUE(content.find("\"repo1\"") != std::string::npos);
        EXPECT_TRUE(content.find("\"repo2\"") != std::string::npos);
        EXPECT_TRUE(content.find("\"42\"") != std::string::npos);
        EXPECT_TRUE(content.find("\"repo3\"") != std::string::npos);
    }

    TEST_F(ProductDbTest, WriteAndReadDb) {
        test_db.add_repo_id("69", "repo1");
        test_db.add_repo_id("69", "repo2");
        test_db.add_repo_id("42", "repo3");

        EXPECT_TRUE(test_db.write_product_db());

        ProductDb new_db;
        new_db.path = test_db.path;
        EXPECT_TRUE(new_db.read_product_db());
        EXPECT_EQ(new_db.repo_map.size(), 2);
        EXPECT_TRUE(new_db.has_product_id("69"));
        EXPECT_TRUE(new_db.has_product_id("42"));
        EXPECT_TRUE(new_db.has_repo_id("69", "repo1"));
        EXPECT_TRUE(new_db.has_repo_id("69", "repo2"));
        EXPECT_TRUE(new_db.has_repo_id("42", "repo3"));
    }
}

namespace test_add_repo_id {
    TEST_F(ProductDbTest, AddNewRepo) {
        EXPECT_TRUE(test_db.add_repo_id("69", "repo1"));
        EXPECT_TRUE(test_db.has_product_id("69"));
        EXPECT_TRUE(test_db.has_repo_id("69", "repo1"));
        EXPECT_EQ(test_db.repo_map["69"].size(), 1);
    }

    TEST_F(ProductDbTest, AddDuplicateRepo) {
        EXPECT_TRUE(test_db.add_repo_id("69", "repo1"));
        EXPECT_FALSE(test_db.add_repo_id("69", "repo1"));
        EXPECT_EQ(test_db.repo_map["69"].size(), 1);
        EXPECT_TRUE(test_db.has_repo_id("69", "repo1"));
    }

    TEST_F(ProductDbTest, AddMultipleRepos) {
        EXPECT_TRUE(test_db.add_repo_id("69", "repo1"));
        EXPECT_TRUE(test_db.add_repo_id("69", "repo2"));
        EXPECT_TRUE(test_db.add_repo_id("42", "repo3"));

        EXPECT_EQ(test_db.repo_map.size(), 2);
        EXPECT_EQ(test_db.repo_map["69"].size(), 2);
        EXPECT_EQ(test_db.repo_map["42"].size(), 1);
    }

    TEST_F(ProductDbTest, AddAndPersistRepo) {
        EXPECT_TRUE(test_db.add_repo_id("69", "repo1"));
        EXPECT_TRUE(test_db.write_product_db());

        ProductDb new_db;
        new_db.path = test_db.path;
        EXPECT_TRUE(new_db.read_product_db());
        EXPECT_TRUE(new_db.has_product_id("69"));
        EXPECT_TRUE(new_db.has_repo_id("69", "repo1"));
    }
}

namespace test_get_repo_ids {
    TEST_F(ProductDbTest, GetRepoIdsEmpty) {
        auto repo_ids = test_db.get_repo_ids("69");
        EXPECT_TRUE(repo_ids.empty());
    }

    TEST_F(ProductDbTest, GetRepoIdsPopulated) {
        test_db.add_repo_id("69", "repo1");
        test_db.add_repo_id("69", "repo2");
        test_db.add_repo_id("42", "repo3");

        auto repo_ids = test_db.get_repo_ids("69");
        EXPECT_EQ(repo_ids.size(), 2);
        EXPECT_TRUE(repo_ids.find("repo1") != repo_ids.end());
        EXPECT_TRUE(repo_ids.find("repo2") != repo_ids.end());

        repo_ids = test_db.get_repo_ids("42");
        EXPECT_EQ(repo_ids.size(), 1);
        EXPECT_TRUE(repo_ids.find("repo3") != repo_ids.end());
    }

    TEST_F(ProductDbTest, GetRepoIdsNonExistent) {
        test_db.add_repo_id("69", "repo1");
        auto repo_ids = test_db.get_repo_ids("42");
        EXPECT_TRUE(repo_ids.empty());
    }
}

namespace test_remove_product_id {
    TEST_F(ProductDbTest, RemoveExistingProduct) {
        test_db.add_repo_id("69", "repo1");
        test_db.add_repo_id("69", "repo2");
        test_db.add_repo_id("42", "repo3");

        const auto ret = test_db.remove_product_id("69");

        EXPECT_TRUE(ret);
        EXPECT_FALSE(test_db.has_product_id("69"));
        EXPECT_TRUE(test_db.has_product_id("42"));
        EXPECT_EQ(test_db.repo_map.size(), 1);
    }

    TEST_F(ProductDbTest, RemoveNonExistentProduct) {
        test_db.add_repo_id("69", "repo1");

        const auto ret = test_db.remove_product_id("42");

        EXPECT_FALSE(ret);
        EXPECT_TRUE(test_db.has_product_id("69"));
        EXPECT_EQ(test_db.repo_map.size(), 1);
    }

    TEST_F(ProductDbTest, RemoveAndPersist) {
        test_db.add_repo_id("69", "repo1");
        test_db.add_repo_id("42", "repo3");

        EXPECT_TRUE(test_db.remove_product_id("69"));
        EXPECT_TRUE(test_db.write_product_db());

        ProductDb new_db;
        new_db.path = test_db.path;
        EXPECT_TRUE(new_db.read_product_db());
        EXPECT_FALSE(new_db.has_product_id("69"));
        EXPECT_TRUE(new_db.has_product_id("42"));
        EXPECT_EQ(new_db.repo_map.size(), 1);
    }
}

namespace test_remove_repo_id {
    TEST_F(ProductDbTest, RemoveExistingRepo) {
        test_db.add_repo_id("69", "repo1");
        test_db.add_repo_id("69", "repo2");
        test_db.add_repo_id("42", "repo3");

        const auto ret = test_db.remove_repo_id("69", "repo1");

        EXPECT_TRUE(ret);
        EXPECT_TRUE(test_db.has_product_id("69"));
        EXPECT_FALSE(test_db.has_repo_id("69", "repo1"));
        EXPECT_TRUE(test_db.has_repo_id("69", "repo2"));
        EXPECT_TRUE(test_db.has_repo_id("42", "repo3"));
        EXPECT_EQ(test_db.repo_map["69"].size(), 1);
    }

    TEST_F(ProductDbTest, RemoveNonExistentProduct) {
        test_db.add_repo_id("69", "repo1");

        const auto ret = test_db.remove_repo_id("42", "repo2");

        EXPECT_FALSE(ret);
        EXPECT_TRUE(test_db.has_repo_id("69", "repo1"));
        EXPECT_EQ(test_db.repo_map["69"].size(), 1);
    }

    TEST_F(ProductDbTest, RemoveNonExistentRepo) {
        test_db.add_repo_id("69", "repo1");

        const auto ret = test_db.remove_repo_id("69", "repo2");

        EXPECT_FALSE(ret);
        EXPECT_TRUE(test_db.has_repo_id("69", "repo1"));
        EXPECT_EQ(test_db.repo_map["69"].size(), 1);
    }

    TEST_F(ProductDbTest, RemoveAndPersistRepo) {
        test_db.add_repo_id("69", "repo1");
        test_db.add_repo_id("69", "repo2");

        EXPECT_TRUE(test_db.remove_repo_id("69", "repo1"));
        EXPECT_TRUE(test_db.write_product_db());

        ProductDb new_db;
        new_db.path = test_db.path;
        EXPECT_TRUE(new_db.read_product_db());
        EXPECT_FALSE(new_db.has_repo_id("69", "repo1"));
        EXPECT_TRUE(new_db.has_repo_id("69", "repo2"));
        EXPECT_EQ(new_db.repo_map["69"].size(), 1);
    }
}

namespace test_product_db_to_string {
    TEST_F(ProductDbTest, EmptyDbToString) {
        EXPECT_EQ(test_db.product_db_to_string(), "{}\n");
    }

    TEST_F(ProductDbTest, SingleProductToString) {
        test_db.add_repo_id("69", "repo1");
        EXPECT_EQ(test_db.product_db_to_string(), "{\n\t\"69\" : [ \"repo1\" ]\n}\n");
    }

    TEST_F(ProductDbTest, MultipleProductsToString) {
        test_db.add_repo_id("69", "repo1");
        test_db.add_repo_id("69", "repo2");
        test_db.add_repo_id("42", "repo3");

        std::string result = test_db.product_db_to_string();
        EXPECT_TRUE(result.find("\"69\"") != std::string::npos);
        EXPECT_TRUE(result.find("\"repo1\"") != std::string::npos);
        EXPECT_TRUE(result.find("\"repo2\"") != std::string::npos);
        EXPECT_TRUE(result.find("\"42\"") != std::string::npos);
        EXPECT_TRUE(result.find("\"repo3\"") != std::string::npos);
    }
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
