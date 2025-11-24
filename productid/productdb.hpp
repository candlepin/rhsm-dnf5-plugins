//
// Created by jhnidek on 24.11.25.
//

#ifndef RHSM_DNF5_PLUGINS_PRODUCTDB_H
#define RHSM_DNF5_PLUGINS_PRODUCTDB_H
#include <string>
#include <map>
#include <set>
#include <json/json.h>

#define DEFAULT_PRODUCTDB_FILE "/var/lib/rhsm/productid.json"

class ProductDb {
public:
    explicit ProductDb();
    explicit ProductDb(const std::string &path);
    ~ProductDb();
    std::string path;
    std::map<std::string, std::set<std::string> > repo_map;

    bool read_product_db();
    [[nodiscard]] bool write_product_db() const;
    [[nodiscard]] Json::Value to_json() const;
    bool add_repo_id(const std::string &product_id, const std::string &repo_id);
    [[nodiscard]] std::set<std::string> get_repo_ids(const std::string& product_id);
    bool remove_product_id(const std::string& product_id);
    bool remove_repo_id(const std::string& product_id, const std::string& repo_id);
    [[nodiscard]] bool has_product_id(const std::string& product_id) const;
    [[nodiscard]] bool has_repo_id(const std::string &product_id, const std::string &repo_id) const;
    [[nodiscard]] std::string product_db_to_string() const;
};

#endif //RHSM_DNF5_PLUGINS_PRODUCTDB_H
