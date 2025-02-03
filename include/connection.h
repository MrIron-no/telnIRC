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

#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>
#include <thread>
#include <csignal>

#include "modules.h"

class Modules;

// Externally defined in main.cpp
extern volatile sig_atomic_t stop_program;

class ConnectionManager {
public:
    ConnectionManager(Modules* _mod, const std::string& ip, unsigned int port);
    ~ConnectionManager();

    void Start();
    void Stop();
    void SendData(const std::string& data);

private:
    void ReadLoop();
    std::string receive_message(std::string &buffer);
    void process_received_data(std::string &buffer);

    Modules* mod;
    int sockfd;
    std::string buffer;
    std::thread receive_thread;
};

#endif // CONNECTION_MANAGER_H
