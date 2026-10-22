/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Trust store for MeeGo Harmattan.
//
// The device has no Qt 6 and its Qt 4 cannot verify a certificate chain from
// a thread that does not own the QSslSocket, so the chain is verified with
// mbedTLS directly against the CA bundle that Harmattan installs.

#include "tls_trust.h"

#include <mbedtls/x509_crt.h>

#include <cstddef>

namespace eka2l1::drivers {
    namespace {
        // Harmattan installs the usual Debian-style bundle; the other paths
        // are the names the same content has under on other distributions.
        const char *const ca_bundle_paths[] = {
            "/etc/ssl/certs/ca-certificates.crt",
            "/etc/pki/tls/certs/ca-bundle.crt",
            "/etc/ssl/cert.pem",
        };

        bool load_trust_store(mbedtls_x509_crt &store) {
            for (const char *path : ca_bundle_paths) {
                const int result = mbedtls_x509_crt_parse_file(&store, path);
                if (result == 0 && store.version != 0) {
                    // At least one certificate was read from the bundle.
                    return true;
                }

                mbedtls_x509_crt_free(&store);
                mbedtls_x509_crt_init(&store);
            }

            return false;
        }
    }

    bool verify_system_certificate(const tls_certificate_chain &chain, const std::string &hostname) {
        if (chain.empty() || hostname.empty()) {
            return false;
        }

        mbedtls_x509_crt peer_chain;
        mbedtls_x509_crt trust_store;

        mbedtls_x509_crt_init(&peer_chain);
        mbedtls_x509_crt_init(&trust_store);

        bool valid = false;

        do {
            for (const auto &der : chain) {
                if (mbedtls_x509_crt_parse_der(&peer_chain, der.data(), der.size()) != 0) {
                    break;
                }
            }

            if (peer_chain.version == 0) {
                break;
            }

            if (!load_trust_store(trust_store)) {
                break;
            }

            std::uint32_t flags = 0;
            valid = mbedtls_x509_crt_verify_with_profile(&peer_chain, &trust_store, nullptr,
                &mbedtls_x509_crt_profile_default, hostname.c_str(), &flags, nullptr, nullptr) == 0;
        } while (false);

        mbedtls_x509_crt_free(&peer_chain);
        mbedtls_x509_crt_free(&trust_store);

        return valid;
    }
}
