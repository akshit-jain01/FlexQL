#include "flexql/flexql.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
using namespace std;

struct flexql_db {
    int sock;
};

int flexql_open(const char* host, int port, flexql_db** db) {

    *db = (flexql_db*)malloc(sizeof(flexql_db));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return FLEXQL_ERROR;

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (strcmp(host, "localhost") == 0) {
        inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
    } else {
        inet_pton(AF_INET, host, &server.sin_addr);
    }

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        free(*db);
        return FLEXQL_ERROR;
    }

    (*db)->sock = sock;
    return FLEXQL_OK;
}

int flexql_close(flexql_db* db) {
    if (!db) return FLEXQL_ERROR;

    close(db->sock);
    free(db);

    return FLEXQL_OK;
}

// New function in flexql.cpp:
int flexql_exec_pipeline(flexql_db* db, const char** sqls, int count,
                          flexql_callback callback, void* arg, char** errmsg) {
    // Phase 1: send ALL queries without waiting
    string batch;
    batch.reserve(count * 32);
    for (int i = 0; i < count; i++) {
        string q(sqls[i]);
        if (q.back() != ';') batch += q + ";\n";
        else batch += q + "\n";
    }
    send(db->sock, batch.c_str(), batch.size(), 0);

    // Phase 2: read all responses
    string response;
    response.reserve(count * 64);
    char buf[65536];
    int expected = count;
    int received = 0;
    while (received < expected) {
        int n = read(db->sock, buf, sizeof(buf));
        if (n <= 0) break;
        response.append(buf, n);
        // count sentinels received
        size_t pos = 0;
        while ((pos = response.find("##END##\n", pos)) != string::npos) {
            received++;
            pos += 8;
        }
    }
    return FLEXQL_OK;
}

int flexql_exec(flexql_db* db, const char* sql,
                flexql_callback callback,
                void* arg,
                char** errmsg) {

    if (!db) return FLEXQL_ERROR;

    string query = string(sql);

// ensure semicolon exists
    if (query.back() != ';') {
        query += ';';
    }
    query += "\n";

    size_t total = 0;
    while (total < query.size()) {
        ssize_t sent = send(db->sock, query.c_str() + total, query.size() - total, 0);
        if (sent <= 0) break;
        total += sent;
    }
 
    string response;
    response.reserve(4096);
    char buf[4096];
    while (true) {
        int n = read(db->sock, buf, sizeof(buf));
        if (n <= 0) break;
        response.append(buf, n);
        if (response.size() >= 8 &&
            response.compare(response.size() - 8, 8, "##END##\n") == 0) {
            response.resize(response.size() - 8);  // strip sentinel
            break;
        }
    }
    if (response.empty()) {
        if (errmsg) *errmsg = strdup("Execution failed");
        return FLEXQL_ERROR;
    }

    

    // detect server error
    if (response.find("Error:") != string::npos) {
        if (errmsg) *errmsg = strdup(response.c_str());
        return FLEXQL_ERROR;
    }

    if (!callback) return FLEXQL_OK;

    stringstream ss(response);
    string row;

    while (getline(ss, row)) {

        if (row.empty()) continue;

        vector<string> cols;
        string val;
        stringstream row_ss(row);

        while (row_ss >> val) {
            cols.push_back(val);
        }

        int n = cols.size();

        char** values = new char*[n];
        for (int i = 0; i < n; i++) {
            values[i] = strdup(cols[i].c_str());
        }

        if (callback(arg, n, values, NULL) == 1) {
            for (int i = 0; i < n; i++) free(values[i]);
            delete[] values;
            return FLEXQL_OK;
        }

        for (int i = 0; i < n; i++) free(values[i]);
        delete[] values;
    }
    return FLEXQL_OK; 
}

void flexql_free(char* errmsg) {
    free(errmsg);
}