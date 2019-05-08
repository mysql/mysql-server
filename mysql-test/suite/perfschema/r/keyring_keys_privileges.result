CREATE USER happygilmore@localhost identified by 'happy';
GRANT SELECT ON *.* TO happygilmore@localhost;
SHOW GRANTS;
Grants for happygilmore@localhost
GRANT SELECT ON *.* TO `happygilmore`@`localhost`
SELECT * FROM performance_schema.keyring_keys;
KEY_ID	KEY_OWNER	BACKEND_KEY_ID
REVOKE SELECT ON *.* FROM happygilmore@localhost;
SHOW GRANTS;
Grants for happygilmore@localhost
GRANT USAGE ON *.* TO `happygilmore`@`localhost`
SELECT * FROM performance_schema.keyring_keys;
ERROR 42000: SELECT command denied to user 'happygilmore'@'localhost' for table 'keyring_keys'
GRANT SELECT ON *.* TO happygilmore@localhost;
SHOW GRANTS;
Grants for happygilmore@localhost
GRANT SELECT ON *.* TO `happygilmore`@`localhost`
SELECT * FROM performance_schema.keyring_keys;
KEY_ID	KEY_OWNER	BACKEND_KEY_ID
DROP USER happygilmore@localhost;
