#
# Bug#58669: read_only not enforced on 5.5.x
#
CREATE USER user1@localhost;
CREATE DATABASE db1;
GRANT ALL PRIVILEGES ON db1.* TO user1@localhost;
CREATE TABLE db1.t1(a INT);
SELECT CURRENT_USER();
CURRENT_USER()
user1@localhost
SHOW VARIABLES LIKE "read_only%";
Variable_name	Value
read_only	ON
INSERT INTO db1.t1 VALUES (1);
ERROR HY000: The MySQL server is running with the --read-only option so it cannot execute this statement
DROP DATABASE db1;
DROP USER user1@localhost;
