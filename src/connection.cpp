/**
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of

 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#include <netdb.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

#include "connection.h"
#include "misc.h"

ConnectionManager::ConnectionManager(Modules* _mod, UIManager& _ui, Logger* _logger, std::string& host, unsigned int port, bool useTLS, std::string _caCertFile, std::string _clientCertFile, std::string _clientKeyFile)
    : mod(_mod), ui(_ui), logger(_logger), tls_enabled(useTLS), caCertFile(_caCertFile), clientCertFile(_clientCertFile), clientKeyFile(_clientKeyFile) {
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));  // Initialize hints to zero
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        ui.shutdown();
        std::cerr << RED << "Error in resolving host" << RESET << std::endl;
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        ui.shutdown();
        std::cerr << RED << "Error in socket creation" << RESET << std::endl;
        exit(1);
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        ui.shutdown();
        std::cerr << RED << "Error in connecting to server" << RESET << std::endl;
        exit(1);
    }

    freeaddrinfo(res);

    // Non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);  // Get current socket flags
    if (flags == -1) {
        ui.shutdown();
        std::cerr << RED << "Error getting socket flags" << RESET << std::endl;
        exit(1);
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {  // Set the socket to non-blocking
        ui.shutdown();
        std::cerr << RED << "Error setting socket to non-blocking mode" << RESET << std::endl;
        exit(1);
    }

    ui.print(NC_YELLOW) << "Connecting to " << host << ":" << port << std::endl;

#ifdef HAVE_OPENSSL
    // Initialize TLS if enabled
    if (tls_enabled) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!ssl_ctx) {
            std::cerr << RED << "Error creating SSL context" << RESET << std::endl;
            exit(1);
        }

        // Load certificates and keys
        if (!LoadCertificates()) {
            std::cerr << RED << "Failed to load certificates." << RESET << std::endl;
            exit(1);
        }

        ssl = SSL_new(ssl_ctx);
        SSL_set_fd(ssl, sockfd);
    }
#endif
}

ConnectionManager::~ConnectionManager() {
    Stop();
#ifdef HAVE_OPENSSL
    if (tls_enabled) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
    }
#endif
    close(sockfd);
}

void ConnectionManager::Start() {
    receive_thread = std::thread([this] { MainLoop(); });
}

void ConnectionManager::Stop() {
    if (receive_thread.joinable()) {
        receive_thread.join();
    }
}

void ConnectionManager::SendData(const std::string& data) {
    std::string msg = data + "\r\n";
    writeBuffer.push_back(msg);
    ui.print << get_timestamp() << " <- " << data << std::endl;
    if (logger)
        logger->log("<- " + data);
}

void ConnectionManager::WriteBufferedData() {
    while (!writeBuffer.empty()) {
        const std::string& data = writeBuffer.front();
        ssize_t bytesSent = 0;
        
#ifdef HAVE_OPENSSL
        if (tls_enabled) {
            bytesSent = SSL_write(ssl, data.c_str(), data.size());
            if (bytesSent <= 0) {
                int err = SSL_get_error(ssl, bytesSent);
                if (err == SSL_ERROR_WANT_WRITE) {
                    return;  // Can't send now, retry later
                }
                ui.print(NC_RED) << "TLS Write Error" << std::endl;
                stop_program = 1;
                return;
            }
        } else {
#endif
            bytesSent = send(sockfd, data.c_str(), data.size(), 0);

            if (bytesSent < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    return;  // Can't send now, keep in buffer
                }
                ui.print(NC_RED) << "Error sending message" << std::endl;
                stop_program = 1;
                return;
            }
#ifdef HAVE_OPENSSL
        }
#endif

        if (bytesSent < static_cast<ssize_t>(data.size())) {
            // Partial write: Keep the remaining unsent part
            writeBuffer.front() = data.substr(bytesSent);
            return;
        }

        // Fully sent, remove from buffer
        writeBuffer.pop_front();
    }
}

void ConnectionManager::MainLoop() {
    while (!stop_program) {
#ifdef HAVE_OPENSSL
        if (tls_enabled && !tls_handshake_done) {
            if (!PerformTLSHandshake()) {
                continue;  // Keep retrying until handshake completes or fails
            }
        }
#endif
        receive_message(buffer);
        WriteBufferedData();
    }
}

std::string ConnectionManager::receive_message(std::string &buffer) {
    char temp_buffer[512];
    memset(temp_buffer, 0, sizeof(temp_buffer));
    ssize_t bytes_received = 0;
    
#ifdef HAVE_OPENSSL
    if (tls_enabled) {
        bytes_received = SSL_read(ssl, temp_buffer, sizeof(temp_buffer) - 1);
        if (bytes_received <= 0) {
            int err = SSL_get_error(ssl, bytes_received);
            if (err == SSL_ERROR_WANT_READ) {
                return "";
            }
            ui.print(NC_RED) << "TLS Read Error" << std::endl;
            stop_program = 1;
            return "";
        }
    } else {
#endif
        bytes_received = recv(sockfd, temp_buffer, sizeof(temp_buffer) - 1, 0);
    
        if (bytes_received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return "";
            }
            ui.print(NC_RED) << "Error receiving message" << std::endl;
            close(sockfd);
            stop_program = 1;  // Set stop flag in main.cpp
            return "";
        }
#ifdef HAVE_OPENSSL
    }
#endif

    if (bytes_received == 0) {
        ui.print(NC_RED) << "Connection closed by server" << std::endl;
        close(sockfd);
        stop_program = 1;
        return "";
    }
    
    buffer += temp_buffer;
    process_received_data(buffer);
    return buffer;
}

void ConnectionManager::process_received_data(std::string &buffer) {
    std::string::size_type end;
    while ((end = buffer.find("\r\n")) != std::string::npos) {
        std::string line = buffer.substr(0, end);
        mod->Parse(line);
        buffer.erase(0, end + 2);
    }
}

#ifdef HAVE_OPENSSL
bool ConnectionManager::PerformTLSHandshake() {
    if (tls_handshake_retries >= 10) {  // Limit retries
        ui.print(NC_RED) << "TLS handshake failed after multiple attempts. Exiting." << std::endl;
        stop_program = true;
        return false;
    }

    int ret = SSL_connect(ssl);
    if (ret == 1) {
        ui.print(NC_YELLOW) << "TLS handshake successful!" << std::endl;

        ui.print(NC_BLUE) << "Negotiated TLS Version: " << SSL_get_version(ssl) << std::endl;

        STACK_OF(X509_NAME)* ca_list = SSL_get_client_CA_list(ssl);
        if (ca_list) {
            ui.print(NC_YELLOW) << "Server requested a client certificate." << std::endl;
        } else {
            ui.print(NC_YELLOW) << "WARNING: Server did NOT request a client certificate." << std::endl;
        }


        X509* clientCert = SSL_get_certificate(ssl);
        if (clientCert) {
            ui.print(NC_YELLOW) << "Client certificate sent successfully." << std::endl;
            X509_free(clientCert);
        } else {
            ui.print(NC_YELLOW) << "Client certificate was NOT sent to the server!" << std::endl;
        }

        // Verify the server certificate
        X509* cert = SSL_get_peer_certificate(ssl);
        if (cert) {
            long verify_result = SSL_get_verify_result(ssl);

            if (verify_result == X509_V_OK) {
                ui.print(NC_YELLOW) << "Server certificate verified successfully." << std::endl;
            } 
            else if (verify_result == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
                ui.print(NC_YELLOW) << "WARNING: Accepting self-signed server certificate." << std::endl;
            } 
            else {
                std::cerr   << RED << "Certificate verification failed: "
                            << X509_verify_cert_error_string(verify_result) << RESET << std::endl;
                X509_free(cert);
                stop_program = true;
                return false;
            }
            char* subject = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
            char* issuer = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);

            ui.print(NC_YELLOW) << "Server Certificate Details:" << std::endl;
            ui.print(NC_YELLOW) << "   Subject: " << (subject ? subject : "Unknown") << std::endl;
            ui.print(NC_YELLOW) << "   Issuer: " << (issuer ? issuer : "Unknown") << std::endl;

            OPENSSL_free(subject);
            OPENSSL_free(issuer);
            X509_free(cert);
        } else {
            ui.print(NC_YELLOW) << "No server certificate was provided!" << std::endl;
            stop_program = true;
            return false;
        }

        tls_handshake_done = true;
        return true;
    }

    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        tls_handshake_retries++;  // Count failed attempts
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return false; // Handshake still in progress, retry later
    }

    ui.print(NC_RED) << "TLS handshake failed with error: " << err << std::endl;
    SSL_free(ssl);
    SSL_CTX_free(ssl_ctx);
    stop_program = true;
    return false;
}

static int TLSVerifyCallback(int preverify_ok, X509_STORE_CTX* ctx) {
    int error = X509_STORE_CTX_get_error(ctx);

    if (error == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
        return 1;  // Allow the self-signed certificate
    }

    if (!preverify_ok) {
        std::cerr << RED << "Certificate verification failed: " << X509_verify_cert_error_string(error) << RESET << std::endl;
    }

    return preverify_ok;  // Return original verification result
}

bool ConnectionManager::LoadCertificates() {
    if (!clientCertFile.empty()) {
        if (SSL_CTX_use_certificate_chain_file(ssl_ctx, clientCertFile.c_str()/*, SSL_FILETYPE_PEM*/) <= 0) {
            ui.print(NC_RED) << "Failed to load client certificate from " << clientCertFile << std::endl;
            return false;
        }
    }

    if (!clientKeyFile.empty()) {
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, clientKeyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
            ui.print(NC_RED) << "Failed to load client private key from " << clientKeyFile << std::endl;
            return false;
        }

        // Ensure private key matches the certificate
        if (!SSL_CTX_check_private_key(ssl_ctx)) {
            ui.print(NC_RED) << "Client private key does not match the certificate!" << std::endl;
            return false;
        }
    }

    // Enable server certificate verification
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, TLSVerifyCallback);
    SSL_CTX_set_verify_depth(ssl_ctx, 1);

    return true;
}
#endif
