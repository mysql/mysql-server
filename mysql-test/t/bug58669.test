
--echo #
--echo # Bug#58669: read_only not enforced on 5.5.x
--echo #

CREATE USER user1@localhost;
CREATE DATABASE db1;
GRANT ALL PRIVILEGES ON db1.* TO user1@localhost;
CREATE TABLE db1.t1(a INT);

connect (con1,localhost,user1,,);
connection con1;
SELECT CURRENT_USER();
SHOW VARIABLES LIKE "read_only%";
--error ER_OPTION_PREVENTS_STATEMENT
INSERT INTO db1.t1 VALUES (1);

connection default;
disconnect con1;
DROP DATABASE db1;
DROP USER user1@localhost;
