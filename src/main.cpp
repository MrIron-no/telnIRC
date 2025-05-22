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

#include <ncurses.h>
#include <string>
#include <csignal>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>

#include "telnirc.h"
#include "telnerv.h"
#include "UIManager.h"

volatile sig_atomic_t stop_program = 0;

void handle_signal(int) { stop_program = 1; }

void handle_resize(int) { UIManager::resized = 1; }

int main(int argc, char *argv[]) {
    // Ignore SIGPIPE
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;  // Ignore SIGPIPE
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, nullptr);

    // Handle termination signals
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGWINCH, handle_resize);

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

    // Initialize ncurses
    UIManager ui;
    ui.init();

    switch (mode) {
        case 'c':
            module = new telnIRC(configFile, ui);
            break;
        case 's':
            module = new telnERV(configFile, ui);
            break;
        default:
            return 1;
    }

    // Print banner.
    module->Banner();

    // Attach.
    module->Attach();

    std::string input_line;
    int cursor_x = 3;
    bool need_redraw_output = true;

    while (!stop_program) {
        bool resized_flag = false;
        int ch = ui.getInput(input_line, cursor_x, resized_flag);

        if (resized_flag) {
            need_redraw_output = true;
            ui.redrawInput(input_line, cursor_x);
            continue;
        }

        switch (ch) {
            case '\n':
                module->OnCommand(input_line);
                input_line.clear();
                cursor_x = 3;
                ui.scrollToBottom();
                need_redraw_output = true;
                break;
            case KEY_BACKSPACE:
            case 127:
            case 8:
                if (!input_line.empty()) input_line.pop_back();
                break;
            case KEY_PPAGE: ui.scrollPageUp(); need_redraw_output = true; break;
            case KEY_NPAGE: ui.scrollPageDown(); need_redraw_output = true; break;
            case KEY_UP:    ui.scrollUp(); need_redraw_output = true; break;
            case KEY_DOWN:  ui.scrollDown(); need_redraw_output = true; break;
            default:
                if (isprint(ch)) input_line += static_cast<char>(ch);
                break;
        }

        ui.clampScroll();
        if (need_redraw_output) {
            ui.redrawOutput();
            need_redraw_output = false;
        }
        ui.redrawInput(input_line, cursor_x);
    }

    // After stop_program is set, join the thread
    module->Detach();

    ui.shutdown();

    // Clean up.
    delete module; module = nullptr;

    std::cout << "Program exiting ..." << std::endl;
}