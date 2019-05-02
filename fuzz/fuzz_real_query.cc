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

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    MYSQL mysql;

    mysql_init(&mysql);
    mysql.options.protocol = MYSQL_PROTOCOL_FUZZ;
    // The fuzzing takes place on network data received from server
    sock_initfuzz(Data,Size);
    if (!mysql_real_connect(&mysql,"localhost","root","root","",0,NULL,0)) {
        return 0;
    }

    mysql_real_query(&mysql, "SELECT * FROM Cars",(ulong)strlen("SELECT * FROM Cars"));

    mysql_close(&mysql);
    return 0;
}
