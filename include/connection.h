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

#pragma once

#include <string>
#include <thread>
#include <csignal>
#include <mutex>
#include <deque>

#include "logger.h"
#include "modules.h"
#include "UIManager.h"
#include "defs.h"

#ifdef HAVE_OPENSSL
    #include <openssl/ssl.h>
    #include <openssl/err.h>
#endif

class Modules;

// Externally defined in main.cpp
extern volatile sig_atomic_t stop_program;

class ConnectionManager {
public:
    ConnectionManager(  Modules* _mod, UIManager& _ui, 
                        Logger* _logger, std::string& ip, 
                        unsigned int port, bool useTLS,
                        std::string _caCertFile, 
                        std::string _clientCertFile, 
                        std::string _clientKeyFile);
    ~ConnectionManager();

    void Start();
    void Stop();
    void SendData(const std::string& data);

private:
    void MainLoop();
    void WriteBufferedData();
    std::string receive_message(std::string &buffer);
    void process_received_data(std::string &buffer);
#ifdef HAVE_OPENSSL
    bool PerformTLSHandshake();
    bool LoadCertificates();
#endif

    Modules* mod;
    UIManager& ui;
    Logger* logger;
    int sockfd;
    std::string buffer;
    std::deque<std::string> writeBuffer;
    std::thread receive_thread;

    bool tls_enabled = false;
    bool tls_handshake_done = false;
    std::string caCertFile;
    std::string clientCertFile;
    std::string clientKeyFile;

#ifdef HAVE_OPENSSL
    unsigned int tls_handshake_retries = 0;
    SSL_CTX* ssl_ctx;
    SSL* ssl;
#endif
};
