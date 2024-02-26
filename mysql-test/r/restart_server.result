RESTART;
ERROR HY000: Restart server failed (mysqld is not managed by supervisor process).
# Verifying RESTART using mysqld_safe as supervisor process.
# Executing a sql command after RESTART.
SELECT 1;
1
1
CREATE USER u1@localhost;
GRANT SHUTDOWN ON *.* TO u1@localhost;
RESTART;
REVOKE SHUTDOWN ON *.* FROM u1@localhost;
FLUSH PRIVILEGES;
RESTART;
ERROR 42000: Access denied; you need (at least one of) the SHUTDOWN privilege(s) for this operation
DROP USER u1@localhost;
