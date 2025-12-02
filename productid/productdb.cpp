//
// Created by jhnidek on 24.11.25.
//

#include <json/json.h>

#include "productdb.hpp"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <ranges>

/// We do not read the product db file in constructors. It is necessary
/// to read the file explicitly using read_product_db() to be able to
/// get an error when it is not possible to read the file.
ProductDb::ProductDb() {
    path = DEFAULT_PRODUCTDB_FILE;
    products = std::map<std::string, ProductRecord>();
}

ProductDb::ProductDb(const std::string &path) {
    this->path = path;
    products = std::map<std::string, ProductRecord>();
}

ProductDb::~ProductDb() {
    ;
}

/// Try to read the JSON document containing the product database. The content of the file
/// could look like this:
///
/// {
///   "37080": [
///     "repo_id_awesome-modifier-37080",
///     "repo_id_foo-x86_64-37080"
///   ],
///   "99000": [
///     "repo_id_awesome-i686-99000"
///   ]
/// }
///
bool ProductDb::read_product_db() {
    if (path.empty()) {
        throw std::runtime_error("Productdb file path is empty");
    }

    Json::Value root;
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error("Unable to open productdb file: " + path);
    }
    
    std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
        );

    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::String errors;
    if (!reader->parse(file_content.c_str(),
                       file_content.c_str() + file_content.length(),
                       &root,
                       &errors)) {
        file.close();
        throw std::runtime_error("Unable to parse productdb file: '" +  path + "': " + errors);
    }
    file.close();
    products.clear();

    if (!root.isObject()) {
        throw std::runtime_error("The productdb file: '" + path + "' root value is not collection");
    }

    for (const auto &product_id: root.getMemberNames()) {
        products[product_id] = ProductRecord(product_id);

        const Json::Value &repos = root[product_id];
        if (!repos.isArray()) {
            products.clear();
            throw std::runtime_error("The productdb file: '" + path + "' has invalid format (value of collection is not array)");
        }

        for (const auto &repo: repos) {
            if (!repo.isString()) {
                products.clear();
                throw std::runtime_error("The productdb file: '" + path + "' has invalid format (value of array is not string)");
            }
            auto repo_id = repo.asString();
            products[product_id].add_repo_id(repo_id);
        }
    }

    return true;
}

/// Convert product database to JSON format
Json::Value ProductDb::to_json() const {
    Json::Value root;
    if (products.empty()) {
        root = Json::objectValue;
    } else {
        for (const auto &product: products | std::views::values) {
            if (product.is_installed) {
                Json::Value repo_array(Json::arrayValue);
                for (const auto &repo: product.repos | std::views::values) {
                    repo_array.append(repo.repo_id);
                }
                root[product.product_id] = repo_array;
            }
        }
    }
    return root;
}

/// Try to write productdb database to JSON file
bool ProductDb::write_product_db() const {

    if (path.empty()) {
        return false;
    }

    auto root = to_json();

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    auto stream_writer_builder = Json::StreamWriterBuilder();
    stream_writer_builder["commentStyle"] = "None";
    stream_writer_builder["indentation"] = "   ";
    stream_writer_builder["prettyPrinting"] = true;
    std::unique_ptr<Json::StreamWriter> stream_writer(stream_writer_builder.newStreamWriter());
    stream_writer->write(root, &file);

    file.close();
    return true;
}

/// Try to add product_id in the products
bool ProductDb::add_product_id(const std::string &product_id, const std::string &product_cert_path) {
    return products.insert({product_id, ProductRecord(product_id, product_cert_path)}).second;
}

/// Try to remove product with given product_id from the products (used)
bool ProductDb::remove_product_id(const std::string& product_id) {
    return products.erase(product_id) > 0;
}

/// Check if the product_id exists in the repo_map (used)
bool ProductDb::has_product_id(const std::string& product_id) const {
    return products.contains(product_id);
}

/// Try to add repo_id in the products
bool ProductRecord::add_repo_id(const std::string &repo_id) {
    return this->repos.insert({repo_id, RepoRecord(repo_id)}).second;
}

/// Try to remove repo_id from the repo_map[product_id]
bool ProductRecord::remove_repo_id(const std::string& repo_id) {
    return this->repos.erase(repo_id) > 0;
}

/// Check if the repo_id exists in the repo_map[product_id]
bool ProductRecord::has_repo_id(const std::string &repo_id) const {
    return this->repos.contains(repo_id);
}
