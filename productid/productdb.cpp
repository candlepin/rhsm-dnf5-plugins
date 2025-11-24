//
// Created by jhnidek on 24.11.25.
//

#include <json/json.h>

#include "productdb.hpp"

#include <fstream>

/// We do not read the product db file in constructors. It is necessary
/// to read the file explicitly using read_product_db() to be able to
/// get an error when it is not possible to read the file.
ProductDb::ProductDb() {
    path = DEFAULT_PRODUCTDB_FILE;
    repo_map = std::map<std::string, std::set<std::string> >();
}

ProductDb::ProductDb(const std::string &path) {
    this->path = path;
    repo_map = std::map<std::string, std::set<std::string> >();
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
        return false;
    }

    Json::Value root;
    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }
    
    std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
        );

    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    if (!reader->parse(file_content.c_str(),
                       file_content.c_str() + file_content.length(),
                       &root,
                       nullptr)) {
        file.close();
        return false;
    }
    repo_map.clear();

    for (const auto &product_id: root.getMemberNames()) {
        std::set<std::string> repo_ids;
        const Json::Value &repos = root[product_id];

        for (const auto &repo: repos) {
            repo_ids.insert(repo.asString());
        }

        repo_map[product_id] = repo_ids;
    }

    file.close();
    return true;
}

/// Convert product database to JSON format
Json::Value ProductDb::to_json() const {
    Json::Value root;
    if (repo_map.empty()) {
        root = Json::objectValue;
    } else {
        for (const auto &[product_id, repo_ids]: repo_map) {
            Json::Value repo_array(Json::arrayValue);
            for (const auto &repo_id: repo_ids) {
                repo_array.append(repo_id);
            }
            root[product_id] = repo_array;
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

/// Try to add repo_id in the repo_map
bool ProductDb::add_repo_id(const std::string &product_id, const std::string &repo_id) {
    if (auto [fst, snd] = repo_map[product_id].insert(repo_id); !snd) {
        return false;
    }
    return true;
}

/// Try to get repo_ids from the repo_map fir given product_id
std::set<std::string> ProductDb::get_repo_ids(const std::string& product_id) {
    if (repo_map.contains(product_id)) {
        return repo_map[product_id];
    }
    return {};
}

/// Try to remove product_id from the repo_map
bool ProductDb::remove_product_id(const std::string& product_id) {
    return repo_map.erase(product_id) > 0;
}

/// Try to remove repo_id from the repo_map[product_id]
bool ProductDb::remove_repo_id(const std::string& product_id, const std::string& repo_id) {
    if (!repo_map.contains(product_id)) {
        return false;
    }
    return repo_map[product_id].erase(repo_id) > 0;
}

/// Check if the product_id exists in the repo_map
bool ProductDb::has_product_id(const std::string& product_id) const {
    return repo_map.contains(product_id);
}

/// Check if the product_id exists in the repo_map and repo_id exists in the repo_map[product_id]
bool ProductDb::has_repo_id(const std::string& product_id, const std::string& repo_id) const {
    return repo_map.contains(product_id) && repo_map.at(product_id).contains(repo_id);
}

/// Convert the product database to a string representation
std::string ProductDb::product_db_to_string() const {
    std::stringstream ss;
    const auto root = to_json();
    Json::StyledStreamWriter stream_writer;
    stream_writer.write(ss, root);
    return ss.str();
}
