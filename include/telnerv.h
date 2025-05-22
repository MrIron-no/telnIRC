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

class telnERV : public Modules {
public:
    telnERV(const std::string&, UIManager&);
    ~telnERV();

    void Attach() override;
    void Detach() override;
    void OnCommand(std::string) override;
    bool Parse(const std::string&) override;
    void Banner() const override;

private:
    Logger* logger = nullptr;

    /* Config variables. */
    std::string uplink;
    unsigned short port;
    std::string password;
    unsigned short intYY;
    std::string serverName;
    bool use_tls = false;
    std::string caCertFile;
    std::string clientCertFile;
    std::string clientKeyFile;
    std::string log_file;

    std::string serverYY;
    std::string uplinkYY;
    std::string uplinkName;

    /* Ticker for bursted clients. */
    unsigned short int clients = 0;

    void burstClient(std::string, std::string, std::string, std::string = "+i");
    void show_help() const;

};
