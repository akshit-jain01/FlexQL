#include "flexql/flexql.h"
#include <iostream>
#include <chrono>
#include <sstream>

using namespace std;
using namespace std::chrono;

#define TOTAL_ROWS 1000000   // adjust later (start small)
#define BATCH_SIZE 5000

int main() {
    flexql_db* db = nullptr;

    if (flexql_open("127.0.0.1", 9000, &db) != FLEXQL_OK) {
        cout << "Failed to connect\n";
        return 1;
    }

    cout << "Connected to FlexQL\n";

    char* err = nullptr;

    // ===== CREATE =====
    flexql_exec(db,
        "CREATE TABLE BIG_USERS(ID INT, NAME VARCHAR, BALANCE DECIMAL);",
        nullptr, nullptr, &err);

    // ===== INSERT BENCH =====
    cout << "\n[INSERT BENCHMARK]\n";

    auto start = high_resolution_clock::now();

    long long inserted = 0;

    while (inserted < TOTAL_ROWS) {

        stringstream ss;
        ss << "INSERT INTO BIG_USERS VALUES ";

        int count = 0;

        while (count < BATCH_SIZE && inserted < TOTAL_ROWS) {
            ss << "("
               << inserted
               << ", user" << inserted
               << ", " << (1000 + (inserted % 1000))
               << ")";

            inserted++;
            count++;

            if (count < BATCH_SIZE && inserted < TOTAL_ROWS)
                ss << ",";
        }

        ss << ";";

        if (flexql_exec(db, ss.str().c_str(), nullptr, nullptr, &err) != FLEXQL_OK) {
            cout << "Insert failed: " << (err ? err : "unknown") << endl;
            return 1;
        }

        if (inserted % (TOTAL_ROWS / 10) == 0) {
            cout << "Progress: " << inserted << "/" << TOTAL_ROWS << "\n";
        }
    }

    auto end = high_resolution_clock::now();

    long long elapsed = duration_cast<milliseconds>(end - start).count();
    long long throughput = (TOTAL_ROWS * 1000LL) / elapsed;

    cout << "\nINSERT DONE\n";
    cout << "Rows: " << TOTAL_ROWS << "\n";
    cout << "Time: " << elapsed << " ms\n";
    cout << "Throughput: " << throughput << " rows/sec\n";

    // ===== SELECT BENCH =====
    cout << "\n[SELECT BENCHMARK]\n";

    auto s1 = high_resolution_clock::now();

    flexql_exec(db,
        "SELECT * FROM BIG_USERS;",
        nullptr, nullptr, &err);

    auto s2 = high_resolution_clock::now();

    cout << "Full scan time: "
         << duration_cast<milliseconds>(s2 - s1).count()
         << " ms\n";

    // ===== WHERE BENCH =====
    cout << "\n[WHERE BENCHMARK]\n";

    auto w1 = high_resolution_clock::now();

    flexql_exec(db,
        "SELECT * FROM BIG_USERS WHERE ID = 1000;",
        nullptr, nullptr, &err);

    auto w2 = high_resolution_clock::now();

    cout << "Indexed lookup time: "
         << duration_cast<microseconds>(w2 - w1).count()
         << " us\n";

    // ===== RANGE BENCH =====
    auto r1 = high_resolution_clock::now();

    flexql_exec(db,
        "SELECT * FROM BIG_USERS WHERE BALANCE > 1500;",
        nullptr, nullptr, &err);

    auto r2 = high_resolution_clock::now();

    cout << "Range query time: "
         << duration_cast<milliseconds>(r2 - r1).count()
         << " ms\n";

    // ===== JOIN BENCH =====
    cout << "\n[JOIN BENCHMARK]\n";

    flexql_exec(db,
        "CREATE TABLE ORDERS(ID INT, USER_ID INT, AMOUNT DECIMAL);",
        nullptr, nullptr, &err);

    // insert some orders
    for (int i = 0; i < 50000; i++) {
        stringstream ss;
        ss << "INSERT INTO ORDERS VALUES("
           << i << ", " << (i % 1000) << ", " << (i * 2) << ");";

        flexql_exec(db, ss.str().c_str(), nullptr, nullptr, &err);
    }

    auto j1 = high_resolution_clock::now();

    flexql_exec(db,
        "SELECT * FROM BIG_USERS JOIN ORDERS ON BIG_USERS.ID = ORDERS.USER_ID;",
        nullptr, nullptr, &err);

    auto j2 = high_resolution_clock::now();

    cout << "Join time: "
         << duration_cast<milliseconds>(j2 - j1).count()
         << " ms\n";

    flexql_close(db);

    return 0;
}