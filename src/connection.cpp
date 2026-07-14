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
#include <sys/socket.h>
#include <chrono>
#include <thread>
#include <random>
#include <sstream>

#include "connection.h"
#include "misc.h"

ConnectionManager::ConnectionManager(Modules* _mod, UIManager& _ui, Logger* _logger, const HostConfig& _host,
    bool useTLS, std::string _caCertFile, std::string _clientCertFile, std::string _clientKeyFile)
    : mod(_mod), ui(_ui), logger(_logger), host(_host),
      tls_enabled(useTLS || host.implicit_tls), caCertFile(_caCertFile),
      clientCertFile(_clientCertFile), clientKeyFile(_clientKeyFile) {
    websocket_mode = host.transport == HostConfig::Transport::WebSocket;

    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai_err = getaddrinfo(host.hostname.c_str(), std::to_string(host.port).c_str(), &hints, &res);
    if (gai_err != 0) {
        ui.fatal("Error resolving host " + host.hostname + ": " + gai_strerror(gai_err));
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        ui.fatal("Error creating socket: " + std::string(strerror(errno)));
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        ui.fatal("Error connecting to " + host.original + ": " + strerror(errno));
    }

    freeaddrinfo(res);

    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        ui.fatal("Error getting socket flags: " + std::string(strerror(errno)));
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        ui.fatal("Error setting socket to non-blocking mode: " + std::string(strerror(errno)));
    }

    ui.print(NC_YELLOW) << "Connecting to " << host.original << std::endl;

    if (tls_enabled) {
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!ssl_ctx) {
            ui.fatal("Error creating SSL context");
        }

        if (!LoadCertificates()) {
            ui.fatal("Failed to load TLS certificates");
        }

        ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            ui.fatal("Error creating SSL object");
        }
        SSL_set_fd(ssl, sockfd);
        SSL_set_tlsext_host_name(ssl, host.hostname.c_str());
    }
}

ConnectionManager::~ConnectionManager() {
    Stop();
    cleanup_tls();
    close(sockfd);
}

void ConnectionManager::cleanup_tls() {
    if (ssl) {
        if (tls_handshake_done)
            SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = nullptr;
    }
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
    if (websocket_mode)
        writeBuffer.push_back(data);
    else
        writeBuffer.push_back(data + "\r\n");

    ui.print << get_timestamp() << " <- " << data << std::endl;
    if (logger)
        logger->log("<- " + data);
}

ssize_t ConnectionManager::transport_write(const char* buf, size_t len) {
    if (tls_enabled) {
        ssize_t bytesSent = SSL_write(ssl, buf, len);
        if (bytesSent <= 0) {
            int err = SSL_get_error(ssl, bytesSent);
            if (err == SSL_ERROR_WANT_WRITE)
                return -1;
            ui.print(NC_RED) << "TLS Write Error" << std::endl;
            stop_program = 1;
            return -1;
        }
        return bytesSent;
    }
    ssize_t bytesSent = send(sockfd, buf, len, 0);
    if (bytesSent < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return -1;
        ui.print(NC_RED) << "Error sending message" << std::endl;
        stop_program = 1;
        return -1;
    }
    return bytesSent;
}

ssize_t ConnectionManager::transport_read(char* buf, size_t len) {
    if (tls_enabled) {
        ssize_t bytes_received = SSL_read(ssl, buf, len);
        if (bytes_received <= 0) {
            int err = SSL_get_error(ssl, bytes_received);
            if (err == SSL_ERROR_WANT_READ)
                return -1;
            ui.print(NC_RED) << "TLS Read Error" << std::endl;
            stop_program = 1;
            return -1;
        }
        return bytes_received;
    }
    ssize_t bytes_received = recv(sockfd, buf, len, 0);
    if (bytes_received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return -1;
        ui.print(NC_RED) << "Error receiving message" << std::endl;
        close(sockfd);
        stop_program = 1;
        return -1;
    }
    return bytes_received;
}

std::string ConnectionManager::encode_websocket_frame(const std::string& payload) {
    std::string frame;
    frame.reserve(payload.size() + 14);
    frame.push_back(static_cast<char>(0x81)); // FIN + text frame

    size_t len = payload.size();
    unsigned char mask_bit = 0x80;

    if (len < 126) {
        frame.push_back(static_cast<char>(mask_bit | static_cast<unsigned char>(len)));
    } else if (len <= 0xFFFF) {
        frame.push_back(static_cast<char>(mask_bit | 126));
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    } else {
        frame.push_back(static_cast<char>(mask_bit | 127));
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
    }

    unsigned char masking_key[4];
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 255);
    for (auto& byte : masking_key)
        byte = static_cast<unsigned char>(dist(gen));

    frame.append(reinterpret_cast<char*>(masking_key), sizeof(masking_key));

    for (size_t i = 0; i < len; ++i)
        frame.push_back(static_cast<char>(payload[i] ^ masking_key[i % 4]));

    return frame;
}

void ConnectionManager::WriteBufferedData() {
    while (!writeBuffer.empty()) {
        const std::string& data = writeBuffer.front();
        std::string wire_data = websocket_mode ? encode_websocket_frame(data) : data;
        ssize_t bytesSent = transport_write(wire_data.c_str(), wire_data.size());

        if (bytesSent < 0)
            return;

        if (bytesSent < static_cast<ssize_t>(wire_data.size())) {
            if (websocket_mode) {
                ui.print(NC_RED) << "Partial WebSocket write not supported" << std::endl;
                stop_program = 1;
            } else {
                writeBuffer.front() = data.substr(bytesSent);
            }
            return;
        }

        writeBuffer.pop_front();
    }
}

bool ConnectionManager::decode_websocket_frames(std::string& ws_buffer) {
    while (ws_buffer.size() >= 2) {
        unsigned char b0 = static_cast<unsigned char>(ws_buffer[0]);
        unsigned char b1 = static_cast<unsigned char>(ws_buffer[1]);
        bool fin = (b0 & 0x80) != 0;
        unsigned char opcode = b0 & 0x0F;
        bool masked = (b1 & 0x80) != 0;
        uint64_t payload_len = b1 & 0x7F;
        size_t header_len = 2;

        if (payload_len == 126) {
            if (ws_buffer.size() < 4)
                return false;
            payload_len = (static_cast<uint64_t>(static_cast<unsigned char>(ws_buffer[2])) << 8) |
                          static_cast<unsigned char>(ws_buffer[3]);
            header_len = 4;
        } else if (payload_len == 127) {
            if (ws_buffer.size() < 10)
                return false;
            payload_len = 0;
            for (int i = 0; i < 8; ++i)
                payload_len = (payload_len << 8) | static_cast<unsigned char>(ws_buffer[2 + i]);
            header_len = 10;
        }

        size_t mask_len = masked ? 4 : 0;
        if (ws_buffer.size() < header_len + mask_len + payload_len)
            return false;

        const unsigned char* mask = masked
            ? reinterpret_cast<const unsigned char*>(ws_buffer.data() + header_len)
            : nullptr;
        size_t payload_offset = header_len + mask_len;

        if (opcode == 0x8) {
            ui.print(NC_RED) << "WebSocket connection closed by server" << std::endl;
            stop_program = 1;
            return false;
        }

        if (opcode == 0x9 || opcode == 0xA) {
            ws_buffer.erase(0, payload_offset + payload_len);
            continue;
        }

        if (opcode != 0x1 && opcode != 0x2 && opcode != 0x0) {
            ws_buffer.erase(0, payload_offset + payload_len);
            continue;
        }

        std::string payload(payload_len, '\0');
        for (uint64_t i = 0; i < payload_len; ++i) {
            unsigned char byte = static_cast<unsigned char>(ws_buffer[payload_offset + i]);
            if (mask)
                byte ^= mask[i % 4];
            payload[static_cast<size_t>(i)] = static_cast<char>(byte);
        }

        ws_buffer.erase(0, payload_offset + payload_len);

        if (fin) {
            std::string parsed_line = payload;
            strip_ircv3_message_tags(parsed_line);
            mod->Parse(payload, parsed_line);
        }
    }

    return true;
}

bool ConnectionManager::PerformWebSocketHandshake() {
    if (ws_handshake_retries >= 50) {
        ui.print(NC_RED) << "WebSocket handshake failed after multiple attempts. Exiting." << std::endl;
        stop_program = true;
        return false;
    }

    if (ws_key.empty()) {
        ws_key = generate_websocket_key();
        std::ostringstream request;
        request << "GET " << host.path << " HTTP/1.1\r\n"
                << "Host: " << host.hostname << ":" << host.port << "\r\n"
                << "Upgrade: websocket\r\n"
                << "Connection: Upgrade\r\n"
                << "Sec-WebSocket-Key: " << ws_key << "\r\n"
                << "Sec-WebSocket-Version: 13\r\n"
                << "Sec-WebSocket-Protocol: text.ircv3.net, binary.ircv3.net\r\n"
                << "\r\n";

        std::string req = request.str();
        ssize_t sent = transport_write(req.c_str(), req.size());
        if (sent < 0) {
            ws_handshake_retries++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return false;
        }
        if (sent < static_cast<ssize_t>(req.size())) {
            ui.print(NC_RED) << "Failed to send WebSocket handshake request" << std::endl;
            stop_program = true;
            return false;
        }
    }

    char temp_buffer[512];
    ssize_t bytes_received = transport_read(temp_buffer, sizeof(temp_buffer) - 1);
    if (bytes_received < 0) {
        ws_handshake_retries++;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return false;
    }

    if (bytes_received == 0) {
        ui.print(NC_RED) << "Connection closed during WebSocket handshake" << std::endl;
        stop_program = true;
        return false;
    }

    ws_buffer.append(temp_buffer, bytes_received);

    size_t header_end = ws_buffer.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return false;

    std::string response = ws_buffer.substr(0, header_end);
    ws_buffer.erase(0, header_end + 4);

    if (response.find("HTTP/1.1 101") == std::string::npos &&
        response.find("HTTP/1.0 101") == std::string::npos) {
        ui.print(NC_RED) << "WebSocket handshake rejected by server" << std::endl;
        stop_program = true;
        return false;
    }

    size_t accept_pos = response.find("Sec-WebSocket-Accept:");
    if (accept_pos == std::string::npos) {
        ui.print(NC_RED) << "WebSocket handshake missing Sec-WebSocket-Accept" << std::endl;
        stop_program = true;
        return false;
    }

    size_t value_start = response.find(':', accept_pos) + 1;
    size_t value_end = response.find("\r\n", value_start);
    std::string accept_value = response.substr(value_start, value_end - value_start);
    while (!accept_value.empty() && accept_value.front() == ' ')
        accept_value.erase(0, 1);
    while (!accept_value.empty() && accept_value.back() == ' ')
        accept_value.pop_back();

    std::string expected = sha1_base64(ws_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    if (accept_value != expected) {
        ui.print(NC_RED) << "WebSocket handshake accept key mismatch" << std::endl;
        stop_program = true;
        return false;
    }

    ui.print(NC_YELLOW) << "WebSocket handshake successful!" << std::endl;
    ws_handshake_done = true;
    return true;
}

void ConnectionManager::MainLoop() {
    while (!stop_program) {
        if (tls_enabled && !tls_handshake_done) {
            if (!PerformTLSHandshake())
                continue;
        }
        if (websocket_mode && !ws_handshake_done) {
            if (!PerformWebSocketHandshake())
                continue;
        }

        receive_message(buffer);
        WriteBufferedData();
    }
}

std::string ConnectionManager::receive_message(std::string &buffer) {
    char temp_buffer[512];
    memset(temp_buffer, 0, sizeof(temp_buffer));
    ssize_t bytes_received = transport_read(temp_buffer, sizeof(temp_buffer) - 1);

    if (bytes_received < 0)
        return "";

    if (bytes_received == 0) {
        ui.print(NC_RED) << "Connection closed by server" << std::endl;
        close(sockfd);
        stop_program = 1;
        return "";
    }

    if (websocket_mode) {
        ws_buffer.append(temp_buffer, bytes_received);
        decode_websocket_frames(ws_buffer);
        return buffer;
    }

    buffer += temp_buffer;
    process_received_data(buffer);
    return buffer;
}

void ConnectionManager::process_received_data(std::string &buffer) {
    std::string::size_type end;
    while ((end = buffer.find("\r\n")) != std::string::npos) {
        std::string display_line = buffer.substr(0, end);
        std::string parsed_line = display_line;
        strip_ircv3_message_tags(parsed_line);
        mod->Parse(display_line, parsed_line);
        buffer.erase(0, end + 2);
    }
}

bool ConnectionManager::PerformTLSHandshake() {
    if (tls_handshake_retries >= 10) {
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
        tls_handshake_retries++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return false;
    }

    ui.print(NC_RED) << "TLS handshake failed";
    while (unsigned long ssl_err = ERR_get_error()) {
        char buf[256];
        ERR_error_string_n(ssl_err, buf, sizeof(buf));
        ui.print(NC_RED) << "  " << buf << std::endl;
    }
    cleanup_tls();
    stop_program = true;
    return false;
}

static int TLSVerifyCallback(int preverify_ok, X509_STORE_CTX* ctx) {
    int error = X509_STORE_CTX_get_error(ctx);

    if (error == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
        return 1;
    }

    if (!preverify_ok) {
        std::cerr << RED << "Certificate verification failed: " << X509_verify_cert_error_string(error) << RESET << std::endl;
    }

    return preverify_ok;
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

        if (!SSL_CTX_check_private_key(ssl_ctx)) {
            ui.print(NC_RED) << "Client private key does not match the certificate!" << std::endl;
            return false;
        }
    }

    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, TLSVerifyCallback);
    SSL_CTX_set_verify_depth(ssl_ctx, 1);

    if (!caCertFile.empty()) {
        if (SSL_CTX_load_verify_locations(ssl_ctx, caCertFile.c_str(), nullptr) != 1) {
            ui.print(NC_RED) << "Failed to load CA certificate from " << caCertFile << std::endl;
            return false;
        }
    } else if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
        ui.print(NC_RED) << "Failed to load system CA certificates" << std::endl;
        return false;
    }

    return true;
}
