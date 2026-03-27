#include "flexql/flexql.h"
#include <stdio.h>
#include <stdlib.h>

// 🔥 callback function
int callback(void* data, int cols, char** values, char** colNames) {
    for (int i = 0; i < cols; i++) {
        printf("%s ", values[i]);
    }
    printf("\n");
    return 0;
}

int main() {
    flexql_db* db;
    char* err = NULL;

    // 🔹 open connection
    if (flexql_open("127.0.0.1", 9000, &db) != FLEXQL_OK) {
        printf("Failed to connect\n");
        return 1;
    }

    // 🔹 create table
    flexql_exec(db,
        "CREATE TABLE A (id INT, name VARCHAR);",
        NULL, NULL, &err);

    // 🔹 insert data
    flexql_exec(db,
        "INSERT INTO A VALUES (1,Alice);",
        NULL, NULL, &err);

    flexql_exec(db,
        "INSERT INTO A VALUES (2,Bob);",
        NULL, NULL, &err);

    // 🔥 SELECT with callback
    printf("Query Result:\n");
    flexql_exec(db,
        "SELECT * FROM A;",
        callback, NULL, &err);

    // 🔹 error test
    if (flexql_exec(db,
        "SELECT * FROM X;",
        callback, NULL, &err) == FLEXQL_ERROR) {

        printf("Error: %s\n", err);
        flexql_free(err);
    }

    // 🔹 close
    flexql_close(db);

    return 0;
}