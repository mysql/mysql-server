#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <mysql.h>
#include <mysql/client_plugin.h>
#include <mysqld_error.h>
#include "violite.h"

using namespace std;
FILE *logfile = NULL;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    MYSQL mysql;
    bool opt_cleartext = true;
    unsigned int opt_ssl = SSL_MODE_DISABLED;
    MYSQL_RES *result;
    uint8_t op;

    if (Size < 1) {
        return 0;
    }
    op = Data[0];
    Data++;
    Size--;
    if (logfile == NULL) {
        logfile = fopen("/dev/null", "w");
    }
    mysql_init(&mysql);
    if (mysql_options(&mysql, MYSQL_ENABLE_CLEARTEXT_PLUGIN, &opt_cleartext) != 0) {
        abort();
    }
    if (mysql_options(&mysql, MYSQL_OPT_SSL_MODE, &opt_ssl) != 0) {
        abort();
    }
    unsigned int my_protocol = MYSQL_PROTOCOL_FUZZ;
    if (mysql_options(&mysql, MYSQL_OPT_PROTOCOL, &my_protocol) != 0) {
        abort();
    }
    // The fuzzing takes place on network data received from server
    sock_initfuzz(Data,Size);
    if (!mysql_real_connect(&mysql, "localhost", "user", "pass", "db", 0, NULL, 0)) {
        goto out;
    }

    mysql_info(&mysql);
    mysql_ping(&mysql);

    switch (op) {
        case 0:
            mysql_query(&mysql, "CREATE DATABASE fuzz");
            if (mysql_query(&mysql, "SELECT * FROM crashes")) {
                goto out;
            }
            result = mysql_store_result(&mysql);
            if (result != NULL) {
                mysql_result_metadata(result);
                mysql_num_rows(result);
                int num_fields = mysql_num_fields(result);
                MYSQL_FIELD *field;
                while((field = mysql_fetch_field(result))) {
                    fprintf(logfile, "%s\n", field->name);
                }
                MYSQL_ROW row = mysql_fetch_row(result);
                unsigned long * lengths = mysql_fetch_lengths(result);
                while (row ) {
                    for(int i = 0; i < num_fields; i++) {
                        fprintf(logfile, "length %lu, %s\n", lengths[i], row[i] ? row[i] : "NULL");
                    }
                    row = mysql_fetch_row(result);
                }
                mysql_free_result(result);
            }
            break;
        case 1:
            result = mysql_list_dbs(&mysql, NULL);
            if (result) {
                mysql_free_result(result);
            }
            break;
        case 2:
            result = mysql_list_tables(&mysql, NULL);
            if (result) {
                mysql_free_result(result);
            }
            break;
        case 3:
            result = mysql_list_fields(&mysql, "table", NULL);
            if (result) {
                mysql_free_result(result);
            }
            break;
        case 4:
            result = mysql_list_processes(&mysql);
            if (result) {
                mysql_free_result(result);
            }
            break;
        case 5:
            if (mysql_query(&mysql, "INSERT INTO Fuzzers(Name) VALUES('target')") == 0) {
                fprintf(logfile, "The last inserted row id is: %llu\n", mysql_insert_id(&mysql));
                fprintf(logfile, "%llu affected rows\n", mysql_affected_rows(&mysql));
            }
            break;
        case 6:
            if (mysql_change_user(&mysql, "user", "password", "new_database")) {
                goto out;
            }
            break;
        case 7:
            if (mysql_select_db(&mysql, "new_database")) {
                goto out;
            }
            break;
    }

    mysql_get_host_info(&mysql);
    mysql_get_proto_info(&mysql);
    mysql_get_server_info(&mysql);
    mysql_get_server_version(&mysql);
    mysql_dump_debug_info(&mysql);
    mysql_sqlstate(&mysql);
    mysql_stat(&mysql);
    mysql_info(&mysql);

out:
    mysql_close(&mysql);
    return 0;
}
