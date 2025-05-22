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

#include "modules.h"

class telnIRC : public Modules {
public:
    telnIRC(const std::string&, UIManager&);
    ~telnIRC();

    void Attach() override;
    void Detach() override;
    void OnCommand(std::string) override;
    bool Parse(const std::string&) override;
    void Banner() const override;

private:
    /* Configuration variables. */
    std::string server_name;
    unsigned short port;
    std::string password;
    std::string nickname;
    std::string username;
    std::string log_file;
    bool use_cap;
    bool use_tls;
    std::string caCertFile;
    std::string clientCertFile;
    std::string clientKeyFile;

    std::string currentBuffer; // Global variable to store the current buffer
    Logger* logger = nullptr;

    void handle_privmsg(const std::string&);
    void show_help();

};
