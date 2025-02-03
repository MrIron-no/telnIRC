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

#ifndef TELNERV_H
#define TELNERV_H

#include "modules.h"

class telnERV : public Modules {
public:
    telnERV(const std::string&);
    ~telnERV();

    void Attach() override;
    void StartLoop() override;
    bool Parse(const std::string&) override;
    void Banner() const override;

private:
    /* Config variables. */
    std::string uplink;
    unsigned short port;
    std::string password;
    unsigned short intYY;
    std::string serverName;

    std::string serverYY;
    std::string uplinkYY;
    std::string uplinkName;

    /* Ticker for bursted clients. */
    unsigned short int clients = 0;

    void burstClient(std::string, std::string, std::string, std::string = "+i");
    void show_help() const;

};

#endif // TELNERV_H
