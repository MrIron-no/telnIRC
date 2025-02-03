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

#ifndef MODULE_BASE_H
#define MODULE_BASE_H

#include <string>

#include "connection.h"

class ConnectionManager;

class Modules {
public:
    ConnectionManager* conn = nullptr;

    Modules(const std::string&) {};
    virtual ~Modules() = default;

    // Pure virtual functions for derived classes to implement
    virtual void Attach() = 0;
    virtual void StartLoop() = 0;
    virtual bool Parse(const std::string&) = 0;
    virtual void Banner() const = 0;
};

#endif // MODULE_BASE_H
