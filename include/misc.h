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
#include <vector>

#define MAX_LOG_LINES 1000

using Params = std::vector<std::string>;
Params Tokenizer(const std::string&);

std::string get_timestamp();
std::string get_unix_username();
std::string generate_random_number_string(size_t);

// ANSI escape codes for colors
const std::string BLUE = "\033[34m";
const std::string RED = "\033[31m";
const std::string YELLOW = "\033[33m";
const std::string RESET = "\033[0m";
