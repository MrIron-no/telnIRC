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

#include "config.h"
#include "misc.h"
#include "connection.h"
#include "numeric.h"
#include "telnerv.h"
#include "UIManager.h"

telnERV::telnERV(const std::string& configFile, UIManager& ui)
    : Modules(configFile, ui) {

    /* Load configuration. */
    ConfigParser config;
    if (!config.load(configFile, "telnERV")) {
        ui.shutdown();
        std::cerr << "Error loading configuration file: " << configFile << std::endl;
        exit(1);
    }

    uplink = config.get<std::string>("server_ip", "127.0.0.1");
    port = config.get<unsigned short>("port", 4400);
    password = config.get<std::string>("password", "password");
    intYY = config.get<unsigned short>("numeric", 51);
    serverName = config.get<std::string>("server_name", "telnerv.undernet.org");
    use_tls = config.get<bool>("tls", false);
    caCertFile = config.get<std::string>("tls_cacert", "");
    clientCertFile = config.get<std::string>("tls_cert", "");
    clientKeyFile = config.get<std::string>("tls_key", "");
}

telnERV::~telnERV() {
    delete conn; conn = nullptr;
}

void telnERV::Attach() {
    char buf[3];
    inttobase64(buf, intYY, 2);
    serverYY = buf;
    ui.print(NC_YELLOW) << "My YY: " << serverYY << std::endl;

    // Initiate connection.
    conn = new ConnectionManager(this, ui, logger, uplink, port
#ifdef HAVE_OPENSSL
        , use_tls, caCertFile, clientCertFile, clientKeyFile
#endif
    );

    conn->Start();

    conn->SendData("PASS :" + password);
    conn->SendData("SERVER " + serverName + " 0 " + std::to_string(time(nullptr)) + " " + std::to_string(time(nullptr)) + " J10 " + serverYY + "]]] + :telnERV");
}

void telnERV::Detach() {
    conn->Stop();
}

void telnERV::OnCommand(std::string input) {
    if (input == "/h") {
        show_help();
    } else if (input.rfind("/n ", 0) == 0) {
        Params params = Tokenizer(input.substr(3));
        if( params.size() > 3)
            burstClient(params[0], params[1], params[2], params[3]);
        else if( params.size() > 2)
            burstClient(params[0], params[1], params[2]);                
    } else if (input.rfind("/sq", 0) == 0) {
        std::string message = "Leaving...";
        if (input.size() > 3) message = input.substr(4);
        conn->SendData(serverYY + " SQ " + uplinkName + " :" + message);
        stop_program = 1;
    } else {
        conn->SendData(input);
    }
}

void telnERV::Banner() const {
    ui.print << "######################################" << std::endl;
    ui.print << "#                                    #" << std::endl;
    ui.print << "#            telnERV                 #" << std::endl;
    ui.print << "#                                    #" << std::endl;
    ui.print << "######################################" << std::endl;
}

void telnERV::burstClient(std::string nick, std::string user, std::string host, std::string modes) {
    char XXX[4];
    inttobase64(XXX, clients, 3);
    conn->SendData(serverYY + " N " + nick + " " + std::to_string(time(nullptr)) + " " + std::to_string(time(nullptr)) + " " + user + " " + host + " " + modes + " AAAAA " + serverYY + XXX + " :telnERV client");
    clients++;

    ui.print(NC_RED) << "Bursted client " << nick << "!" << user << "@" << host << std::endl;
}

bool telnERV::Parse(const std::string &line) {
    ui.print << get_timestamp() << " -> " << line << std::endl;

    Params params = Tokenizer(line);

    // msg_SERVER
    if (params.size() > 7 && params[0] == "SERVER") {
        uplinkName = params[1];
        uplinkYY = params[6].substr(0, 2);

        // Output the results
        ui.print(NC_YELLOW) << "Uplink Name: " << uplinkName << std::endl;
        ui.print(NC_YELLOW) << "Uplink YY: " << uplinkYY << std::endl;

        return true;
    }

    // msg_EB
    if (params.size() > 1 &&
        params[1] == "EB" &&
        params[0] == uplinkYY) {
        
        /* Complete burst. */
        conn->SendData(serverYY + " EB");
        conn->SendData(serverYY + " EA");

        ui.print(NC_YELLOW) << "Burst completed." << std::endl;

        return true;
    }

    // msg_G
    // AB G !1736027261.141624 server.name 1736027261.141624
    // A3 Z A3 !1736027261.141624 1736027261.141624 0 1736027261.141800
    if (params.size() > 4 && params[1] == "G") {
        std::stringstream message;
        message << serverYY
                << " Z "
                << params[0]
                << " "
                << params[2]
                << " "
                << params[4]
                << " 0 "
                << params[4];

        conn->SendData(message.str());
        return true;
    }

    return false;
}

void telnERV::show_help() const {
    ui.print << "Available Commands:" << std::endl;
    ui.print << "/h                              - Show this help message" << std::endl;
    ui.print << "/sq [msg]                       - Squits" << std::endl;
    ui.print << "/n <nick> <user> <host> [modes] - Bursts a new client" << std::endl;
}