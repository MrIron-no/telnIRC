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

#include <string>
#include <csignal>
#include <cstring>
#include <iostream>

#include "telnirc.h"
#include "telnerv.h"

volatile sig_atomic_t stop_program = 0;

void handle_signal(int signal) {
    stop_program = 1;
    std::cout << "\nProgram terminated by signal " << signal << "\n";
    exit(signal);
}

int main(int argc, char *argv[]) {
    // Handle termination signals
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::string configFile = "config.cfg";

    bool hasMode = false;  // Ensure at least one of -s or -c is present
    char mode = '\0';      // Stores either 's' or 'c'

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 < argc) {
                configFile = argv[i + 1];
                ++i; // Skip next argument since it's the config file
            } else {
                std::cerr << "Error: Missing argument for -f option\n";
                return 1;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
            if (hasMode) {
                std::cerr << "Error: Cannot specify both -s and -c\n";
                return 1;
            }
            mode = 's';
            hasMode = true;
        } else if (strcmp(argv[i], "-c") == 0) {
            if (hasMode) {
                std::cerr << "Error: Cannot specify both -s and -c\n";
                return 1;
            }
            mode = 'c';
            hasMode = true;
        } else {
            std::cerr << "Error: Unknown option '" << argv[i] << "'\n";
            return 1;
        }
    }

    // Ensure at least -s or -c is provided
    if (!hasMode) {
        std::cerr   << "SYNTAX: " << argv[0] << " [-f file] -c | -s\n"
                    << "\t-f: Config file. Default: config.cfg\n"
                    << "\t-s: Spins up a server\n"
                    << "\t-c: Spins up a client" << std::endl;
        return 1;
    }

    Modules* module = nullptr;

    switch (mode) {
        case 'c':
            module = new telnIRC(configFile);
            break;
        case 's':
            module = new telnERV(configFile);
            break;
        default:
            return 1;
    }

    // Print banner.
    module->Banner();

    // Attach.
    module->Attach();

    // Start loop.
    module->StartLoop();

    // Clean up.
    delete module; module = nullptr;

    std::cout << "Program exiting ..." << std::endl;
}