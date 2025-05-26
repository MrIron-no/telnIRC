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

#include <regex>

#include "config.h"
#include "misc.h"
#include "connection.h"
#include "telnirc.h"
#include "UIManager.h"
#include "logger.h"

telnIRC::telnIRC(const std::string& configFile, UIManager& ui)
    : Modules(configFile, ui) {

    /* Load configuration. */
    ConfigParser config;
    if (!config.load(configFile, "telnIRC")) {
        ui.shutdown();
        std::cerr << "Error loading configuration file: " << configFile << std::endl;
        exit(1);
    }

    server_name = config.get<std::string>("server_ip", "127.0.0.1");
    port = config.get<unsigned int>("port", 4400);
    password = config.get<std::string>("password", "");
    nickname = config.get<std::string>("nick", get_unix_username());
    username = config.get<std::string>("user", nickname);
    log_file = config.get<std::string>("logfile", "");
    use_cap = config.get<bool>("cap", true);
    use_tls = config.get<bool>("tls", false);
    caCertFile = config.get<std::string>("tls_cacert", "");
    clientCertFile = config.get<std::string>("tls_cert", "");
    clientKeyFile = config.get<std::string>("tls_key", "");
}

telnIRC::~telnIRC() {
    delete conn; conn = nullptr;
}

void telnIRC::Attach() {
    // Initiate logger
    if (!log_file.empty()) {
        logger = new Logger();
        logger->open(log_file);
        logger->log("telnIRC started");
    }

    // Initiate connection.
    conn = new ConnectionManager(this, ui, logger, server_name, port
#ifdef HAVE_OPENSSL
        , use_tls, caCertFile, clientCertFile, clientKeyFile
#endif
    );

    // Start receiving loop in a thread.
    conn->Start();

    if (!password.empty()) {
        conn->SendData("PASS :" + password);
    }

    if (use_cap) {
        conn->SendData("CAP LS");
    }

    conn->SendData("NICK " + nickname);
    conn->SendData("USER " + username + " 0 * :" + nickname);
}

void telnIRC::Detach() {
    conn->Stop();
}

void telnIRC::OnCommand(std::string input) {
    if (input == "/h") {
        show_help();
    } else if (input.rfind("/j ", 0) == 0 && input.size() > 3) {
        std::string channel = input.substr(3);
        conn->SendData("JOIN " + channel);
    } else if (input.rfind("/w ", 0) == 0 && input.size() > 3) {
        std::string nick = input.substr(3);
        conn->SendData("WHOIS " + nick);
    } else if (input.rfind("/p ", 0) == 0 && input.size() > 3) {
        std::string channel = input.substr(3);
        conn->SendData("PART " + channel);
        currentBuffer.clear();
        ui.setHeader("");
    } else if (input.rfind("/r ", 0) == 0  && input.size() > 3) {
        std::string message = input.substr(3);
        conn->SendData(message);
    } else if (input.rfind("/q", 0) == 0) {
        std::string message = "Leaving...";
    if (input.size() > 2) message = input.substr(3);
        conn->SendData("QUIT :" + message);
        stop_program = 1;
    } else if (input.rfind("/n ", 0) == 0 && input.size() > 3) {
        conn->SendData("NICK " + input.substr(3));
    } else if (input.rfind("/msg ", 0) == 0 && input.size() > 5) {
        std::string remainder = input.substr(5);
        size_t first_space = remainder.find(' ');
        if (first_space != std::string::npos) {
            std::string target = remainder.substr(0, first_space);
            std::string message = remainder.substr(first_space + 1);
            conn->SendData("PRIVMSG " + target + " :" + message);
            if (currentBuffer != target) {
                currentBuffer = target; // Update currentBuffer
                ui.print(NC_YELLOW) << "Current buffer updated to: " << currentBuffer << std::endl;
                ui.setHeader("Current buffer: " + currentBuffer);
            }
        }
    } else if (input.rfind("/sb ", 0) == 0) {
        currentBuffer = input.substr(4);
        ui.print(NC_YELLOW) << "Current buffer set to: " << currentBuffer << std::endl;
        ui.setHeader("Current buffer: " + currentBuffer);
    } else if (!input.empty()) {
        if (!currentBuffer.empty()) {
            conn->SendData("PRIVMSG " + currentBuffer + " :" + input);
        } else {
            ui.print(NC_YELLOW) << "No current buffer set. Please join a channel, set a buffer, or receive a direct message first." << std::endl;
        }
    }
}

void telnIRC::Banner() const {
    ui.print << "######################################" << std::endl;
    ui.print << "#                                    #" << std::endl;
    ui.print << "#            telnIRC                 #" << std::endl;
    ui.print << "#                                    #" << std::endl;
    ui.print << "######################################" << std::endl;
}

void telnIRC::handle_privmsg(const std::string &line) {
    // Handle color output first
    bool contains_nickname = line.find(nickname) != std::string::npos;
    if (contains_nickname) {
        ui.print(NC_RED) << get_timestamp() << " "
                << "-> " << line << std::endl;
    } else {
        ui.print(NC_BLUE) << get_timestamp() << " "
                << "-> " << line << std::endl;
    }

    // Extract sender nickname
    size_t start_pos = line.find(":") + 1;
    size_t end_pos = line.find("!");
    std::string sender_nick = line.substr(start_pos, end_pos - start_pos);

    // Handle CTCP commands
    std::smatch ctcp_match;
    std::regex ctcp_regex("\\x01([^\\s]+)(.*)\\x01");

    if (std::regex_search(line, ctcp_match, ctcp_regex)) {
        std::string ctcpCmd = ctcp_match[1];
        std::string ctcpArgs = ctcp_match[2];

        if (ctcpCmd == "VERSION") {
            conn->SendData("NOTICE " + sender_nick + " :\x01VERSION telnIRC - theRealIRC\x01");
            return;
        } else if (ctcpCmd == "PING") {
            conn->SendData("NOTICE " + sender_nick + " :\x01PING " + ctcpArgs + "\x01");
            return;
        }
    }

    // Check if message is directed to us
    std::regex privmsg_regex("^:[^\\s]+![^\\s]+ PRIVMSG " + nickname + " :.*$");
    if (!std::regex_search(line, privmsg_regex)) {
        return;  // Early return if not directed to us
    }

    // We only update the current buffer if the message is directed to us and is not CTCP
    if (currentBuffer != sender_nick) {
        currentBuffer = sender_nick;
        ui.print(NC_YELLOW) << "Current buffer updated to user: " << currentBuffer << std::endl;
        ui.setHeader("Current buffer: " + currentBuffer);
    }
}

bool telnIRC::Parse(const std::string& line) {
    std::smatch match;

    if (logger)
        logger->log("-> " + line);

    // PRIVMSG handling (including color output)
    if (std::regex_search(line, std::regex("^:[^\\s]+ PRIVMSG"))) {
        handle_privmsg(line);
        return true;
    }

    // Non-PRIVMSG messages are printed in default color
    ui.print << get_timestamp() << " -> " << line << std::endl;

    // Welcome message (001)
    if (std::regex_search(line, match, std::regex("^:[^\\s]+ 001 ([^\\s]+)")) &&
        match[1] != nickname) {
        nickname = match[1];
        ui.print << "Nickname updated to: " << nickname << std::endl;
        return true;
    }

    // Nickname in use (433)
    if (std::regex_search(line, std::regex("^:[^\\s]+ 433"))) {
        std::string new_nick = nickname + generate_random_number_string(12 - nickname.length());
        conn->SendData("NICK " + new_nick);
        nickname = new_nick;
        ui.print(NC_YELLOW) << "Nickname in use. Changed to: " << new_nick << std::endl;
        return true;
    }

    // PING response
    if (line.rfind("PING ", 0) == 0) {
        conn->SendData("PONG " + line.substr(5));
        return true;
    }

    // JOIN message
    if (std::regex_search(line, match, std::regex("^:" + nickname + "!.* JOIN (#[^\\s]+)"))) {
        if (currentBuffer != match[1]) {
            currentBuffer = match[1];
            ui.print(NC_YELLOW) << "Current buffer updated to channel: " << currentBuffer << std::endl;
            ui.setHeader("Current buffer: " + currentBuffer);
        }
        return true;
    }

    // NICK change
    if (std::regex_search(line, match, std::regex("^:" + nickname + "!.* NICK :(.*)$"))) {
        nickname = match[1];
        ui.print(NC_YELLOW) << "Nickname updated to: " << nickname << std::endl;
        return true;
    }

    // CAP messages
    if (std::regex_search(line, match, std::regex("^:[^\\s]+ CAP [^\\s]+ LS :(.*)$")) && use_cap) {
        conn->SendData("CAP REQ :" + std::string(match[1]));
        return true;
    }

    if (std::regex_search(line, std::regex("^:[^\\s]+ CAP [^\\s]+ ACK .*$"))) {
        conn->SendData("CAP END");
        return true;
    }

    return false;
}

void telnIRC::show_help() {
    ui.print(NC_YELLOW) << "Available Commands:" << std::endl;
    ui.print(NC_YELLOW) << "/j #channel      - Join a channel" << std::endl;
    ui.print(NC_YELLOW) << "/p #channel      - Part from a channel" << std::endl;
    ui.print(NC_YELLOW) << "/r message       - Send raw message directly to the server" << std::endl;
    ui.print(NC_YELLOW) << "/q message       - Quits with the specified message" << std::endl;
    ui.print(NC_YELLOW) << "/n newnick       - Change your nickname" << std::endl;
    ui.print(NC_YELLOW) << "/w nickname      - Whois a nickname" << std::endl;
    ui.print(NC_YELLOW) << "/msg user msg    - Send a private message to a user or channel (updates currentBuffer)" << std::endl;
    ui.print(NC_YELLOW) << "/sb user/channel - Set the current buffer to a user or channel" << std::endl;
    ui.print(NC_YELLOW) << "/h               - Show this help message" << std::endl;
}