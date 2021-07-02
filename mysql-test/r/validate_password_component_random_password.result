CREATE USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
CREATE USER 'usr2'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr2	localhost	######	1
GRANT ALL ON mysql.* TO 'usr1'@'localhost';
INSTALL COMPONENT "file://component_validate_password";
# password policy LOW (which only check for password length)
# default case: password length should be minimum 8
SET @@global.validate_password.policy=LOW;
SET @@global.generated_random_password_length = 5;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.generated_random_password_length = 8;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.length= 12;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.mixed_case_count= 0;
SET @@global.validate_password.number_count= 0;
SET @@global.validate_password.special_char_count= 0;
SET @@global.validate_password.length= 0;
SET @@global.validate_password.length= DEFAULT;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
# password policy MEDIUM (check for mixed_case, digits, special_chars)
# default case : atleast 1 mixed_case, 1 digit, 1 special_char
SET @@global.validate_password.mixed_case_count= 1;
SET @@global.validate_password.number_count= 1;
SET @@global.validate_password.special_char_count= 1;
SET @@global.validate_password.policy=MEDIUM;
SET @@global.validate_password.number_count= 0;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.mixed_case_count= 0;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.special_char_count= 0;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.special_char_count= 1;
SET @@global.validate_password.number_count= 1;
SET @@global.validate_password.mixed_case_count= 1;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.number_count= 2;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.number_count= 1;
SET @@global.validate_password.mixed_case_count= 2;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.mixed_case_count= 1;
SET @@global.validate_password.special_char_count= 2;
SET @@global.validate_password.special_char_count= 1;
# No dictionary file present, no dictionary check
SET @@global.validate_password.policy=STRONG;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
SET @@global.validate_password.dictionary_file="MYSQL_ERRMSG_BASEDIR/dictionary.txt";
# password policy strong
# default_file : dictionary.txt
SET @@global.validate_password.policy=STRONG;
CREATE USER 'usr3'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr3	localhost	######	1
ALTER USER 'usr1'@'localhost' IDENTIFIED BY RANDOM PASSWORD;
user	host	generated password	auth_factor
usr1	localhost	######	1
SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM;
user	host	generated password	auth_factor
usr1	localhost	######	1
DROP USER 'usr3'@'localhost';
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
@@validate_password.length	@@validate_password.number_count	@@validate_password.mixed_case_count	@@validate_password.special_char_count	@@validate_password.policy	@@validate_password.dictionary_file
8	1	1	1	MEDIUM	NULL
# Cleanup.
UNINSTALL COMPONENT "file://component_validate_password";
End of tests
