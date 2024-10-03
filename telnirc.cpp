#include <iostream>
#include <string>
#include <thread>
#include <regex>
#include <random>
#include <unistd.h> // For getpwuid
#include <pwd.h> // For getpwuid
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>
#include <cstring>
#include <csignal> // For handling signals

std::string currentBuffer = ""; // Global variable to store the current buffer
std::string nickname = ""; // Global variable to store the current nickname

volatile sig_atomic_t stop_program = 0 ;

// ANSI escape codes for colors
const std::string BLUE = "\033[34m";
const std::string RED = "\033[31m";
const std::string RESET = "\033[0m";

void handle_signal(int signal) {
    stop_program = 1;
    std::cout << "\nProgram terminated by signal " << signal << "\n";
//    exit(signal);
}

void show_ascii_banner() {
    std::cout << "######################################" << std::endl;
    std::cout << "#                                    #" << std::endl;
    std::cout << "#            telnIRC                 #" << std::endl;
    std::cout << "#                                    #" << std::endl;
    std::cout << "######################################" << std::endl;
}

std::string generate_random_number_string(size_t length) {
    static const char digits[] = "0123456789";
    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(0, sizeof(digits) - 2);

    std::string result;
    for (size_t i = 0; i < length; ++i) {
        result += digits[dist(generator)];
    }
    return result;
}

int create_socket(const std::string &server_name, const std::string &port) {
    struct addrinfo hints{}, *res;
    int sockfd;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server_name.c_str(), port.c_str(), &hints, &res) != 0) {
        std::cerr << "Error in resolving host" << std::endl;
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        std::cerr << "Error in socket creation" << std::endl;
        exit(1);
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        std::cerr << "Error in connecting to server" << std::endl;
        exit(1);
    }

    freeaddrinfo(res);
    return sockfd;
}

void send_message(int sockfd, const std::string &message) {
    std::string msg = message + "\r\n";
    if (send(sockfd, msg.c_str(), msg.length(), 0) < 0) {
        std::cerr << "Error sending message" << std::endl;
        exit(1);
    }
    std::cout << "[OUT]: " << message << std::endl;
}

void process_received_data(std::string &buffer, int sockfd) {
    std::string::size_type end;
    while ((end = buffer.find("\r\n")) != std::string::npos) {
        std::string line = buffer.substr(0, end);

        // Check if the message is a PRIVMSG
        bool is_privmsg = std::regex_search(line, std::regex(R"(^:[^\s]+ PRIVMSG)"));
        bool contains_nickname = line.find(nickname) != std::string::npos;

        if (is_privmsg && contains_nickname) {
            std::cout << RED << "[IN ]: " << line << RESET << std::endl;
        } else if (is_privmsg) {
            std::cout << BLUE << "[IN ]: " << line << RESET << std::endl;
        } else {
            std::cout << "[IN ]: " << line << std::endl;
        }

        // Check for 433 message using regex
        std::regex nick_in_use_regex(R"(^:[^\s]+ 433)");
        if (std::regex_search(line, nick_in_use_regex)) {
            std::cout << "Nickname in use. Attempting a new one..." << std::endl;
            std::string new_nick = nickname + generate_random_number_string(12 - nickname.length());
            send_message(sockfd, "NICK " + new_nick);
            nickname = new_nick; // Update nickname with the new one
        } else if (line.find("PING ") != std::string::npos) {
            send_message(sockfd, "PONG " + line.substr(5));
        }

        // Detecting JOIN messages to update currentBuffer
        std::regex join_regex("^:" + nickname + R"(!.* JOIN (#[^\s]+))");
        std::smatch match;
        if (std::regex_search(line, match, join_regex)) {
            currentBuffer = match[1];
            std::cout << "Current buffer updated to channel: " << currentBuffer << std::endl;
        }

        // Detecting PRIVMSG directed to us and updating currentBuffer
        std::regex privmsg_regex(R"(^:[^\s]+![^\s]+ PRIVMSG )" + nickname + R"( :.*$)");
        if (std::regex_search(line, privmsg_regex)) {
            size_t start_pos = line.find(":") + 1;
            size_t end_pos = line.find("!");
            std::string sender_nick = line.substr(start_pos, end_pos - start_pos);
            currentBuffer = sender_nick;
            std::cout << "Current buffer updated to user: " << currentBuffer << std::endl;
        }

        // Detecting NICK change for ourselves
        std::regex nick_change_regex("^:" + nickname + R"(!.* NICK :(.*)$)");
        if (std::regex_search(line, match, nick_change_regex)) {
            nickname = match[1];
            std::cout << "Nickname updated to: " << nickname << std::endl;
        }

        // Detecting CAP LS and CAP ACK messages
        std::regex cap_ls_regex(R"(^:[^\s]+ CAP [^\s]+ LS :(.*)$)");
        std::regex cap_ack_regex(R"(^:[^\s]+ CAP [^\s]+ ACK .*$)");

        if (std::regex_search(line, match, cap_ls_regex)) {
            std::string capabilities = match[1];
            send_message(sockfd, "CAP REQ :" + capabilities);
        } else if (std::regex_search(line, cap_ack_regex)) {
            send_message(sockfd, "CAP END");
        }

        buffer.erase(0, end + 2); // Remove the processed line from buffer
    }
}

std::string receive_message(int sockfd, std::string &buffer) {
    char temp_buffer[512];
    memset(temp_buffer, 0, sizeof(temp_buffer));
    ssize_t bytes_received = recv(sockfd, temp_buffer, sizeof(temp_buffer) - 1, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            std::cout << "Connection closed by server" << std::endl;
        } else {
            std::cerr << "Error receiving message" << std::endl;
        }
	close(sockfd);
        exit(0);
    }
    buffer += temp_buffer; // Append the received data to the buffer
    process_received_data(buffer, sockfd); // Process complete lines in buffer
    return buffer;
}

std::string get_unix_username() {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        return std::string(pw->pw_name);
    } else {
        return "unknown";
    }
}

void show_help() {
    std::cout << "Available Commands:\n";
    std::cout << "/j #channel      - Join a channel\n";
    std::cout << "/p #channel      - Part from a channel\n";
    std::cout << "/r message       - Send raw message directly to the server\n";
    std::cout << "/q message       - Quits with the specified message\n";
    std::cout << "/n newnick       - Change your nickname\n";
    std::cout << "/msg user msg    - Send a private message to a user or channel (updates currentBuffer)\n";
    std::cout << "/b user/channel  - Set the current buffer to a user or channel\n";
    std::cout << "/cb              - Show the current buffer\n";
    std::cout << "/h               - Show this help message\n";
}

int main(int argc, char *argv[]) {
    show_ascii_banner();

    // Handle termination signals
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server> [port] [nickname] [-c] [-p <password>]" << std::endl;
        return 1;
    }

    std::string server_name = argv[1];
    std::string port = "6667";
    nickname = get_unix_username();
    bool use_cap = false;
    std::string password;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c") {
            use_cap = true;
        } else if (arg == "-p" && i + 1 < argc) {
            password = argv[++i];
        } else if (isdigit(arg[0])) {
            port = arg;
        } else {
            nickname = arg;
        }
    }

    int sockfd = create_socket(server_name, port);

    if (!password.empty()) {
        send_message(sockfd, "PASS :" + password);
    }

    if (use_cap) {
        send_message(sockfd, "CAP LS");
    }

    send_message(sockfd, "NICK " + nickname);
    send_message(sockfd, "USER " + nickname + " 0 * :" + nickname);

    std::string buffer;

    std::thread receive_thread([sockfd, &buffer]() {
        while (!stop_program) {
            receive_message(sockfd, buffer);
        }
    });

    receive_thread.detach();

    std::string input;
    while (!stop_program) {
        std::getline(std::cin, input);

        if (input == "/h") {
            show_help();
        } else if (input == "/cb") {
            std::cout << "Current Buffer: " << currentBuffer << std::endl;
        } else if (input.rfind("/j ", 0) == 0) {
            std::string channel = input.substr(3);
            send_message(sockfd, "JOIN " + channel);
        } else if (input.rfind("/p ", 0) == 0) {
            std::string channel = input.substr(3);
            send_message(sockfd, "PART " + channel);
        } else if (input.rfind("/r ", 0) == 0) {
            std::string message = input.substr(3);
            send_message(sockfd, message);
        } else if (input.rfind("/q ", 0) == 0) {
            std::string message = input.substr(3);
            send_message(sockfd, "QUIT :" + message);
        } else if (input.rfind("/n ", 0) == 0) {
            std::string newnick = input.substr(3);
            if (newnick.length() <= 12) {
                send_message(sockfd, "NICK " + newnick);
            } else {
                std::cout << "Nickname too long. Please use 12 characters or fewer." << std::endl;
            }
        } else if (input.rfind("/msg ", 0) == 0) {
            std::string remainder = input.substr(5);
            size_t first_space = remainder.find(' ');
            if (first_space != std::string::npos) {
                std::string target = remainder.substr(0, first_space);
                std::string message = remainder.substr(first_space + 1);
                send_message(sockfd, "PRIVMSG " + target + " :" + message);
                currentBuffer = target; // Update currentBuffer
            }
        } else if (input.rfind("/b ", 0) == 0) {
            currentBuffer = input.substr(3);
            std::cout << "Current buffer set to: " << currentBuffer << std::endl;
        } else if (!input.empty()) {
            if (!currentBuffer.empty()) {
                send_message(sockfd, "PRIVMSG " + currentBuffer + " :" + input);
            } else {
                std::cout << "No current buffer set. Please join a channel, set a buffer, or receive a direct message first." << std::endl;
            }
        }
    }

    // After stop_program is set, join the thread
    if (receive_thread.joinable()) {
        receive_thread.join();  // Ensure the thread is cleaned up before exiting
    }

    std::cout << "Program exiting ..." << std::endl;

    return 0;
}
