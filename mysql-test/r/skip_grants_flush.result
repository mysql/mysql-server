FLUSH PRIVILEGES;
SELECT USER(),CURRENT_USER();
USER()	CURRENT_USER()
root@	skip-grants user@skip-grants host
CREATE USER u1@localhost;
ALTER USER u1@localhost IDENTIFIED BY 'pass1';
SET PASSWORD FOR u1@localhost = 'pass2';
SET PASSWORD = 'cant have';
ERROR 42000: Can't find any matching row in the user table
DROP USER u1@localhost;
End of 5.7 tests
