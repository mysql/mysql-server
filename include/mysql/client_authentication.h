#ifndef CLIENT_AUTHENTICATION_H
#define CLIENT_AUTHENTICATION_H
#include <my_global.h>
#include "mysql.h"
#include "mysql/client_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif
int sha256_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql);
void set_path_to_rsa_pk_pem(const char *complete_path);
void sha256_password_options(const char *opt, const void *value);
#ifdef __cplusplus
} // extern "C"
#endif 
#endif

