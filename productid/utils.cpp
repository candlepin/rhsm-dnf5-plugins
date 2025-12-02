//
// Created by jhnidek on 04.12.25.
//

#include <format>
#include <libdnf5/utils/fs/file.hpp>

#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#include "utils.hpp"

/// Try to decompress downloaded compressed productid certificate to some temporary file.
/// This function can raise an exception when it is not possible to read or decompress
/// the given file.
std::string decompress_productid_cert(const std::filesystem::path & compressed_cert_path) {
    // When use_solv_xfopen is equal to true, then libdnf5 will transparently decompress the file
    auto compressed_file = libdnf5::utils::fs::File(compressed_cert_path, "rb", true);
    return compressed_file.read();
}

/// Try to get product ID from certificate content. The ID should be stored in the
/// extension starting with OID: 1.3.6.1.4.1.2312.9.1. There could be several extensions
/// with such OIDs. e.g.:
///
/// 1.3.6.1.4.1.2312.9.1.38091.2 ... containing the version of product cert
/// 1.3.6.1.4.1.2312.9.1.38091.1 ... containing the name of the product
/// 1.3.6.1.4.1.2312.9.1.38091.3 ... containing supported architecture
///
/// We care only about the remaining part of the OID 1.3.6.1.4.1.2312.9.1. In this
/// case it is the number: 38091. This is the product ID we try to return.
std::string get_product_id_from_cert_content(const std::string & cert_content) {
    BIO *bio = BIO_new_mem_buf(cert_content.c_str(), static_cast<int>(cert_content.size()));
    if (bio == nullptr) {
        const std::string err_str(ERR_error_string(ERR_get_error(), nullptr));
        throw std::runtime_error("Unable to create buffer for content of certificate: " + err_str);
    }

    X509 *x509 = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (x509 == nullptr) {
        const std::string err_str(ERR_error_string(ERR_get_error(), nullptr));
        throw std::runtime_error("Failed to read content of certificate from buffer to X509 structure: " + err_str);
    }

    bool redhat_oid_found = false;
    std::string product_id;

    // Go through all extensions of X509 structure and try to
    // find the first REDHAT_PRODUCT_OID extension and return product_id
    // from the extension OID
    const int extensions = X509_get_ext_count(x509);
    for (int i = 0; i < extensions; i++) {
        char oid[MAX_BUFF];
        X509_EXTENSION *ext = X509_get_ext(x509, i);
        if (ext == nullptr) {
            X509_free(x509);
            const std::string err_str(ERR_error_string(ERR_get_error(), nullptr));
            throw std::runtime_error("Failed to get extension of X509 structure: " + err_str);
        }
        OBJ_obj2txt(oid, MAX_BUFF, X509_EXTENSION_get_object(ext), 1);

        if (std::string_view oid_str(oid); oid_str.starts_with(REDHAT_PRODUCT_OID)) {
            oid_str.remove_prefix(strlen(REDHAT_PRODUCT_OID));
            if (const auto end_pos = oid_str.find('.'); end_pos != std::string::npos) {
                product_id = oid_str.substr(0, end_pos);
                redhat_oid_found = true;
                break;
            }
        }
    }

    X509_free(x509);

    if (!redhat_oid_found || product_id.empty()) {
        throw std::runtime_error(std::format("Red Hat Product OID: {} not found or malformed",
            std::string(REDHAT_PRODUCT_OID)));
    }


    return product_id;
}
