#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>

using namespace std;

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9000);

    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    cout << "Connected to server\n";



    while (true) {
        string query, line;
        cout << "flexql> ";
        query.clear();

        while (getline(cin, line)) {
            query += line + " ";

            // stop when semicolon is found
            if (line.find(';') != string::npos)
                break;
        }

        // remove semicolon
        if (query.back() != ';') {
            query += ';';
        }

        // trim
        query.erase(0, query.find_first_not_of(" "));
        query.erase(query.find_last_not_of(" ") + 1);

        if (query == "exit") break;

        query += "\n";   // helps server read cleanly

        size_t total = 0;
        while (total < query.size()) {
            ssize_t sent = send(sock, query.c_str() + total, query.size() - total, 0);
            if (sent <= 0) break;
            total += sent;
        }

        string response;
        char buffer[4096];

        while (true) {
            int bytes = read(sock, buffer, sizeof(buffer));
            if (bytes <= 0) break;

            response.append(buffer, bytes);

            // stop when full response received
            if (response.find("##END##\n") != string::npos)
                break;
        }

        cout << response << endl;
    }

    return 0;
}