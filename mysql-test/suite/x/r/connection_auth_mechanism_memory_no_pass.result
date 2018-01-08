CREATE USER xuser_native@'localhost'
IDENTIFIED WITH 'mysql_native_password' BY '';
CREATE USER xuser_sha256@'localhost'
IDENTIFIED WITH 'sha256_password' BY '';
CREATE USER xuser_cache2@'localhost'
IDENTIFIED WITH 'caching_sha2_password' BY '';
GRANT ALL ON *.* TO xuser_native@localhost;
GRANT ALL ON *.* TO xuser_sha256@localhost;
GRANT ALL ON *.* TO xuser_cache2@localhost;
-- Expecting success with parameters: REQUIRED	plain	xuser_native	
--
connecting...
active session is now 'sess0'
closing session sess0
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
-- Expecting success with parameters: REQUIRED	plain	xuser_sha256	
--
connecting...
active session is now 'sess0'
closing session sess0
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
-- Expecting success with parameters: REQUIRED	plain	xuser_cache2	
--
connecting...
active session is now 'sess0'
closing session sess0
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
-- Expecting success with parameters: DISABLED	memory	xuser_native	
--
connecting...
active session is now 'sess0'
closing session sess0
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
-- Expecting success with parameters: DISABLED	memory	xuser_sha256	
--
connecting...
active session is now 'sess0'
closing session sess0
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
-- Expecting success with parameters: DISABLED	memory	xuser_cache2	
--
connecting...
active session is now 'sess0'
closing session sess0
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP USER xuser_native@'localhost';
DROP USER xuser_sha256@'localhost';
DROP USER xuser_cache2@'localhost';
