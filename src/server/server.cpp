#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <map>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cstring>
#include <string.h>
#include <algorithm>

#include "parser/parser.h"
#include "query/executor.h"

using namespace std;

// ===== Simple Table Storage =====
struct Table {
    vector<string> columns;
    vector<vector<string>> rows;
};

// map<string, Table> database;


// ===== Client Handler =====
void handle_client(int client_socket) {
    char buffer[1<<20];
    string send_buf;
    send_buf.reserve(1 << 20);

    string buffer_accum;   // 🔥 PERSISTENT BUFFER

    while (true) {
        int bytes = read(client_socket, buffer, sizeof(buffer));
        if (bytes <= 0) break;

        buffer_accum.append(buffer, bytes);
        cout << "Buffer size: " << buffer_accum.size() << endl;

        size_t pos;

        // process queries ending with ";\n" OR ";"
        while (true) {
            size_t pos_semicolon = buffer_accum.find(';');

            if (pos_semicolon == string::npos) break;

            // include semicolon
            string query = buffer_accum.substr(0, pos_semicolon + 1);
            buffer_accum.erase(0, pos_semicolon + 1);

            // 🔥 normalize
            replace(query.begin(), query.end(), '\n', ' ');
            replace(query.begin(), query.end(), '\r', ' ');

            query.erase(0, query.find_first_not_of(" "));
            query.erase(query.find_last_not_of(" ") + 1);

            if (query.empty()) continue;

            // 🔥 DEBUG (IMPORTANT)
            cout << "QUERY RECEIVED:\n" << query << "\n---\n";

            string cached;
            if (check_cache(query, cached)) {
                send_buf += cached;
            } else {
                Query q = parse_query(query);
                send_buf += execute_query_obj(q, query);
            }
            send_buf += "##END##\n";
            if (send_buf.size() >= 65536) {
                send(client_socket, send_buf.c_str(), send_buf.size(), 0);
                send_buf.clear();
            }
            if (!send_buf.empty()) {
                send(client_socket, send_buf.c_str(), send_buf.size(), 0);
                send_buf.clear();
            }
        }
    }

    if (!send_buf.empty()) {
        send(client_socket, send_buf.c_str(), send_buf.size(), 0);
    }

    close(client_socket);
}

// ===== Main =====
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(9000);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 5);

    cout << "Server running on port 9000...\n";

    while (true) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        int flag = 1;
        setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        thread(handle_client, new_socket).detach();  // ✅ multithreaded (correct behavior)
    }

    return 0;
}