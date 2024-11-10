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
#include <fcntl.h>
#include <functional>  // for std::bind
#include <cerrno>
#include <iomanip>
#include <ctime>
#include <sstream>

std::string currentBuffer = ""; // Global variable to store the current buffer
std::string nickname = ""; // Global variable to store the current nickname

volatile sig_atomic_t stop_program = 0 ;

// ANSI escape codes for colors
const std::string BLUE = "\033[34m";
const std::string RED = "\033[31m";
const std::string YELLOW = "\033[33m";
const std::string RESET = "\033[0m";

void set_socket_non_blocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);  // Get current socket flags
    if (flags == -1) {
        std::cerr << "Error getting socket flags" << std::endl;
        exit(1);
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {  // Set the socket to non-blocking
        std::cerr << "Error setting socket to non-blocking mode" << std::endl;
        exit(1);
    }
}

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

std::string get_timestamp() {
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << tm->tm_hour << ':'
        << std::setw(2) << tm->tm_min << ':'
        << std::setw(2) << tm->tm_sec;
    return oss.str();
}

int create_socket(const std::string &server_name, const unsigned short port) {
    struct addrinfo hints;
    struct addrinfo *res;
    int sockfd;

    memset(&hints, 0, sizeof(hints));  // Initialize hints to zero
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server_name.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
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
    std::cout << get_timestamp() << " [OUT]: " << message << std::endl;
}

void handle_privmsg(const std::string &line, int sockfd) {
    // Handle color output first
    bool contains_nickname = line.find(nickname) != std::string::npos;
    std::cout << get_timestamp() << " "
              << (contains_nickname ? RED : BLUE)
              << "[IN ]: " << line << RESET << std::endl;

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
            send_message(sockfd, "NOTICE " + sender_nick + " :\x01VERSION telnIRC - theRealIRC\x01");
            return;
        } else if (ctcpCmd == "PING") {
            send_message(sockfd, "NOTICE " + sender_nick + " :\x01PING " + ctcpArgs + "\x01");
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
        std::cout << YELLOW << "Current buffer updated to user: " << currentBuffer << RESET << std::endl;
    }
}

bool process_line(const std::string &line, int sockfd) {
    std::smatch match;

    // PRIVMSG handling (including color output)
    if (std::regex_search(line, std::regex("^:[^\\s]+ PRIVMSG"))) {
        handle_privmsg(line, sockfd);
        return true;
    }

    // Non-PRIVMSG messages are printed in default color
    std::cout << get_timestamp() << " [IN ]: " << line << std::endl;

    // Welcome message (001)
    if (std::regex_search(line, match, std::regex("^:[^\\s]+ 001 ([^\\s]+)")) &&
        match[1] != nickname) {
        nickname = match[1];
        std::cout << "Nickname updated to: " << nickname << std::endl;
        return true;
    }

    // Nickname in use (433)
    if (std::regex_search(line, std::regex("^:[^\\s]+ 433"))) {
        std::string new_nick = nickname + generate_random_number_string(12 - nickname.length());
        send_message(sockfd, "NICK " + new_nick);
        nickname = new_nick;
        std::cout << "Nickname in use. Changed to: " << new_nick << std::endl;
        return true;
    }

    // PING response
    if (line.rfind("PING ", 0) == 0) {
        send_message(sockfd, "PONG " + line.substr(5));
        return true;
    }

    // JOIN message
    if (std::regex_search(line, match, std::regex("^:" + nickname + "!.* JOIN (#[^\\s]+)"))) {
        if (currentBuffer != match[1]) {
            currentBuffer = match[1];
            std::cout << YELLOW << "Current buffer updated to channel: " << currentBuffer << RESET << std::endl;
        }
        return true;
    }

    // NICK change
    if (std::regex_search(line, match, std::regex("^:" + nickname + "!.* NICK :(.*)$"))) {
        nickname = match[1];
        std::cout << "Nickname updated to: " << nickname << std::endl;
        return true;
    }

    // CAP messages
    if (std::regex_search(line, match, std::regex("^:[^\\s]+ CAP [^\\s]+ LS :(.*)$"))) {
        send_message(sockfd, "CAP REQ :" + std::string(match[1]));
        return true;
    }

    if (std::regex_search(line, std::regex("^:[^\\s]+ CAP [^\\s]+ ACK .*$"))) {
        send_message(sockfd, "CAP END");
        return true;
    }

    return false;
}

void process_received_data(std::string &buffer, int sockfd) {
    std::string::size_type end;
    while ((end = buffer.find("\r\n")) != std::string::npos) {
        std::string line = buffer.substr(0, end);

        // Process line
        process_line(line, sockfd);

        buffer.erase(0, end + 2);
    }
}

std::string receive_message(int sockfd, std::string &buffer) {
    char temp_buffer[512];
    memset(temp_buffer, 0, sizeof(temp_buffer));
    ssize_t bytes_received = recv(sockfd, temp_buffer, sizeof(temp_buffer) - 1, 0);
    if (bytes_received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // Non-blocking mode: No data available, so return and check later
            return "";
        } else {
            std::cerr << "Error receiving message" << std::endl;
            close(sockfd);
            stop_program = 1;  // Set stop_program flag
            return "";
        }
    } else if (bytes_received == 0) {
        std::cout << "Connection closed by server" << std::endl;
        close(sockfd);
        stop_program = 1;  // Set stop_program flag
        return "";
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
    std::cout << "/w nickname      - Whois a nickname\n";
    std::cout << "/msg user msg    - Send a private message to a user or channel (updates currentBuffer)\n";
    std::cout << "/b user/channel  - Set the current buffer to a user or channel\n";
    std::cout << "/cb              - Show the current buffer\n";
    std::cout << "/h               - Show this help message\n";
}

class ReceiveHandler {
public:
    ReceiveHandler(int sock, std::string& buf) : sockfd(sock), buffer(buf) {}

    void operator()() {
        while (!stop_program) {
            receive_message(sockfd, buffer);
        }
    }
private:
    int sockfd;
    std::string& buffer;
};

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
    unsigned short port = 6667;
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
            port = std::stoul(arg);
        } else {
            nickname = arg;
        }
    }

    std::cout << "Connecting to " << server_name << ":" << port << std::endl;

    int sockfd = create_socket(server_name, port);
    set_socket_non_blocking(sockfd);

    if (!password.empty()) {
        send_message(sockfd, "PASS :" + password);
    }

    if (use_cap) {
        send_message(sockfd, "CAP LS");
    }

    send_message(sockfd, "NICK " + nickname);
    send_message(sockfd, "USER " + nickname + " 0 * :" + nickname);

    std::string buffer;
    std::thread receive_thread(ReceiveHandler(sockfd, buffer));

    receive_thread.detach();

    std::string input;
    while (!stop_program) {
        if (std::getline(std::cin, input)) {

        if (input == "/h") {
            show_help();
        } else if (input == "/cb") {
            std::cout << YELLOW << "Current Buffer: " << currentBuffer << RESET << std::endl;
        } else if (input.rfind("/j ", 0) == 0 && input.size() > 3) {
            std::string channel = input.substr(3);
            send_message(sockfd, "JOIN " + channel);
        } else if (input.rfind("/w ", 0) == 0 && input.size() > 3) {
            std::string nick = input.substr(3);
            send_message(sockfd, "WHOIS " + nick);
        } else if (input.rfind("/p ", 0) == 0 && input.size() > 3) {
            std::string channel = input.substr(3);
            send_message(sockfd, "PART " + channel);
	        currentBuffer.clear();
        } else if (input.rfind("/r ", 0) == 0  && input.size() > 3) {
            std::string message = input.substr(3);
            send_message(sockfd, message);
        } else if (input.rfind("/q", 0) == 0) {
            std::string message = "Leaving...";
	    if (input.size() > 2) message = input.substr(3);
            send_message(sockfd, "QUIT :" + message);
            stop_program = 1;
        } else if (input.rfind("/n ", 0) == 0 && input.size() > 3) {
            std::string newnick = input.substr(3);
            if (newnick.length() <= 12) {
                send_message(sockfd, "NICK " + newnick);
            } else {
                std::cout << "Nickname too long. Please use 12 characters or fewer." << std::endl;
            }
        } else if (input.rfind("/msg ", 0) == 0 && input.size() > 5) {
            std::string remainder = input.substr(5);
            size_t first_space = remainder.find(' ');
            if (first_space != std::string::npos) {
                std::string target = remainder.substr(0, first_space);
                std::string message = remainder.substr(first_space + 1);
                send_message(sockfd, "PRIVMSG " + target + " :" + message);
		if (currentBuffer != target) {
		    currentBuffer = target; // Update currentBuffer
		    std::cout << YELLOW << "Current buffer updated to: " << currentBuffer << RESET << std::endl;
		}
            }
        } else if (input.rfind("/b ", 0) == 0) {
            currentBuffer = input.substr(3);
            std::cout << YELLOW << "Current buffer set to: " << currentBuffer << RESET << std::endl;
        } else if (!input.empty()) {
            if (!currentBuffer.empty()) {
                send_message(sockfd, "PRIVMSG " + currentBuffer + " :" + input);
            } else {
                std::cout << YELLOW << "No current buffer set. Please join a channel, set a buffer, or receive a direct message first."
			  << RESET << std::endl;
            }
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
