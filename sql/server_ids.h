#ifndef SERVER_ID_H

#define SERVER_ID_H

#include "my_sys.h"

class Server_ids
{
  public:
    DYNAMIC_ARRAY server_ids;

    Server_ids();
    ~Server_ids();

    bool pack_server_ids(char *buffer);
    bool unpack_server_ids(const char *param_server_ids);
};

#endif
