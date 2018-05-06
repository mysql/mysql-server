** Setup **

SET @default_read_only = @@read_only;
'#--------------------FN_DYNVARS_140_01-------------------------#'
SET Global read_only=ON;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name BLOB
);
INSERT into t1(name) values("aaassssssssddddddddffffff");
update t1 set name="jfjdf" where id=1;
select * from t1 where id=1;
id	name
1	jfjdf
'#--------------------FN_DYNVARS_140_02-------------------------#'
** Creating new user with out super privilege**
CREATE user sameea;
** Connecting connn using username 'sameea' **
SET Global read_ONLY=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
CREATE TABLE t2
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name BLOB
);
ERROR HY000: The MySQL server is running with the --read-only option so it cannot execute this statement
not updating values
INSERT into t2(name) values("aaassssssssddddddddffffff");
Got one of the listed errors
UPDATE t2 SET name="samia" where id=1;
Got one of the listed errors
'#--------------------FN_DYNVARS_140_03-------------------------#'
CREATE TEMPORARY TABLE t3(a int);
'#--------------------FN_DYNVARS_140_04-------------------------#'
** Connection default **
SET Global read_only=OFF;
** Connection connn **
CREATE TABLE t2
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name BLOB
);
updating values
INSERT into t2(name) values("aaassssssssdddddddd");
UPDATE t2 SET name="samia" where id=1;
** Connection default **
** Disconnecting connn **
DROP USER sameea;
DROP TABLE t1;
DROP TABLE t2;
SET global read_only = @default_read_only;
