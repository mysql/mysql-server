--source include/have_validate_password_component.inc
--source include/have_debug.inc

let $MYSQL_ERRMSG_BASEDIR=`select @@lc_messages_dir`;

# component is not installed so even 'pass' (very weak) is accepted as
# a password
--replace_column 3 ######
CREATE USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
CREATE USER 'usr2'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
GRANT ALL ON mysql.* TO 'usr1'@'localhost';

INSTALL COMPONENT "file://component_validate_password";

# test for all the three password policy
# policy: LOW, MEDIUM, STRONG

--echo # password policy LOW (which only check for password length)
--echo # default case: password length should be minimum 8

SET @@global.validate_password.policy=LOW;

SET @@global.generated_random_password_length = 5;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.generated_random_password_length = 8;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.validate_password.length= 12;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.validate_password.mixed_case_count= 0;
SET @@global.validate_password.number_count= 0;
SET @@global.validate_password.special_char_count= 0;
SET @@global.validate_password.length= 0;
SET @@global.validate_password.length= DEFAULT;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';


--echo # password policy MEDIUM (check for mixed_case, digits, special_chars)
--echo # default case : atleast 1 mixed_case, 1 digit, 1 special_char

SET @@global.validate_password.mixed_case_count= 1;
SET @@global.validate_password.number_count= 1;
SET @@global.validate_password.special_char_count= 1;
SET @@global.validate_password.policy=MEDIUM;
SET @@global.validate_password.number_count= 0;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.validate_password.mixed_case_count= 0;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.validate_password.special_char_count= 0;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.validate_password.special_char_count= 1;
SET @@global.validate_password.number_count= 1;
SET @@global.validate_password.mixed_case_count= 1;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.validate_password.number_count= 2;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.validate_password.number_count= 1;
SET @@global.validate_password.mixed_case_count= 2;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

SET @@global.validate_password.mixed_case_count= 1;
SET @@global.validate_password.special_char_count= 2;
SET @@global.validate_password.special_char_count= 1;

--echo # No dictionary file present, no dictionary check
SET @@global.validate_password.policy=STRONG;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';

--replace_result $MYSQL_ERRMSG_BASEDIR MYSQL_ERRMSG_BASEDIR
eval SET @@global.validate_password.dictionary_file="$MYSQL_ERRMSG_BASEDIR/dictionary.txt";

--echo # password policy strong
--echo # default_file : dictionary.txt

# file should contain 1 word per line
# error if substring of password is a dictionary word

SET @@global.validate_password.policy=STRONG;
--replace_column 3 ######
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
--replace_column 3 ######
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
DROP USER 'usr3'@'localhost';



connection default;
DROP USER 'usr1'@'localhost';
DROP USER 'usr2'@'localhost';
SET @@global.validate_password.length=default;
SET @@global.validate_password.number_count=default;
SET @@global.validate_password.mixed_case_count=default;
SET @@global.validate_password.special_char_count=default;
SET @@global.validate_password.policy=default;
SET @@global.validate_password.dictionary_file=default;
SET @@global.generated_random_password_length=default;

SELECT @@validate_password.length,
       @@validate_password.number_count,
       @@validate_password.mixed_case_count,
       @@validate_password.special_char_count,
       @@validate_password.policy,
       @@validate_password.dictionary_file;

--echo # Cleanup.
UNINSTALL COMPONENT "file://component_validate_password";
disconnect default;

--echo End of tests

