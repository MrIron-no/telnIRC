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

#include "connection.h"
#include "misc.h"

ConnectionManager::ConnectionManager(Modules* _mod, const std::string& host, unsigned int port)
    : mod(_mod) {
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));  // Initialize hints to zero
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        std::cerr << "Error in resolving host" << std::endl;
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        std::cerr << "Error in socket creation" << std::endl;
        exit(1);
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        std::cerr << "Error in connecting to server" << std::endl;
        exit(1);
    }

    freeaddrinfo(res);

    // Non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);  // Get current socket flags
    if (flags == -1) {
        std::cerr << "Error getting socket flags" << std::endl;
        exit(1);
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {  // Set the socket to non-blocking
        std::cerr << "Error setting socket to non-blocking mode" << std::endl;
        exit(1);
    }

    std::cout << "Connecting to " << host << ":" << port << std::endl;
}

ConnectionManager::~ConnectionManager() {
    Stop();
    close(sockfd);
}

void ConnectionManager::Start() {
    receive_thread = std::thread([this] { ReadLoop(); });
}

void ConnectionManager::Stop() {
    if (receive_thread.joinable()) {
        receive_thread.join();
    }
}

void ConnectionManager::SendData(const std::string& data) {
    std::string msg = data + "\r\n";
    if (send(sockfd, msg.c_str(), msg.length(), 0) < 0) {
        std::cerr << "Error sending message" << std::endl;
        exit(1);
    }
    std::cout << get_timestamp() << " <- " << data << std::endl;
}

void ConnectionManager::ReadLoop() {
    while (!stop_program) {
        receive_message(buffer);
    }
}

std::string ConnectionManager::receive_message(std::string &buffer) {
    char temp_buffer[512];
    memset(temp_buffer, 0, sizeof(temp_buffer));
    ssize_t bytes_received = recv(sockfd, temp_buffer, sizeof(temp_buffer) - 1, 0);
    
    if (bytes_received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return "";
        } else {
            std::cerr << "Error receiving message" << std::endl;
            close(sockfd);
            stop_program = 1;  // Set stop flag in main.cpp
            return "";
        }
    } else if (bytes_received == 0) {
        std::cout << "Connection closed by server" << std::endl;
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