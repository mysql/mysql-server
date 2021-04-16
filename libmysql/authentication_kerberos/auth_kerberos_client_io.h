/* Copyright (c) 2021, Oracle and/or its affiliates.
 */

#ifndef AUTH_KERBEROS_CLIENT_IO_H_
#define AUTH_KERBEROS_CLIENT_IO_H_

#include <mysql/plugin_auth.h>

class Kerberos_client_io {
 public:
  Kerberos_client_io(MYSQL_PLUGIN_VIO *vio);
  ~Kerberos_client_io();
  bool write_gssapi_buffer(const unsigned char *buffer, int buffer_len);
  bool read_gssapi_buffer(unsigned char **gssapi_buffer, size_t *buffer_len);
  bool read_spn_realm_from_server(std::string &service_principal_name,
                                  std::string &upn_realm);

 private:
  /* Plug-in VIO. */
  MYSQL_PLUGIN_VIO *m_vio{nullptr};
};

#endif  // AUTH_KERBEROS_CLIENT_IO_H_
