//
// Created by jhnidek on 02.12.25.
//



#ifndef RHSM_DNF5_PLUGINS_UTILS_HPP
#define RHSM_DNF5_PLUGINS_UTILS_HPP
#include <filesystem>

#define MAX_BUFF 256

// The Red Hat OID plus ".1" which is the product namespace
#define REDHAT_PRODUCT_OID "1.3.6.1.4.1.2312.9.1."

std::string decompress_productid_cert(const std::filesystem::path & compressed_cert_path);

std::string get_product_id_from_cert_content(const std::string & cert_content);

#endif //RHSM_DNF5_PLUGINS_UTILS_HPP
