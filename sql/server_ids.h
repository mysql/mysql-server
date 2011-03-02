#ifndef SERVER_ID_H

#define SERVER_ID_H

#include <my_sys.h>
#include <sql_string.h>

class Server_ids
{
  public:
    DYNAMIC_ARRAY server_ids;

    Server_ids();
    ~Server_ids();

    bool pack_server_ids(String *buffer);
    bool unpack_server_ids(char *param_server_ids);
};

#endif
