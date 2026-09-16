# Temporarily GRANT TP_CONNECTION_ADMIN to root@localhost and
# create the connection admcon which will end up in the admin
# group. This is useful in order to be able to execute queries,
# e.g. against performance_schema without affecting the state
# of the normal thread groups.

GRANT TP_CONNECTION_ADMIN ON *.* TO root@localhost;
connect admcon,localhost,root,,,,,;
connection default;
REVOKE TP_CONNECTION_ADMIN ON *.* FROM root@localhost;
