#
# 1. Column must remain intact due to Global GRANT.
#
SET GLOBAL partial_revokes = ON;
CREATE USER foo;
SELECT user, user_attributes FROM mysql.user WHERE user = 'foo';
user	user_attributes
foo	NULL
GRANT INSERT ON *.* TO foo;
SELECT user, user_attributes FROM mysql.user WHERE user = 'foo';
user	user_attributes
foo	NULL
#
# 2. Column must remain intact after Add/Drop partial revokes from user.
#
REVOKE INSERT ON mysql.* FROM foo;
GRANT INSERT ON mysql.* TO foo;
SELECT user, user_attributes FROM mysql.user WHERE user = 'foo';
user	user_attributes
foo	NULL
DROP USER foo;
#
# 3. Adding/removing partial revokes must not impact other attributes
#
CREATE USER foo@localhost FAILED_LOGIN_ATTEMPTS 4 PASSWORD_LOCK_TIME 6;
ALTER USER foo@localhost ATTRIBUTE '{"key":"value"}';
ALTER USER foo@localhost COMMENT 'Added key/value pair';
SELECT user, user_attributes FROM mysql.user WHERE user='foo';
user	user_attributes
foo	{"metadata": {"key": "value", "comment": "Added key/value pair"}, "Password_locking": {"failed_login_attempts": 4, "password_lock_time_days": 6}}
GRANT SELECT ON *.* TO foo@localhost;
REVOKE SELECT ON mysql.* FROM foo@localhost;
# Pre-existing attributes must not be changed.
SELECT user, user_attributes FROM mysql.user WHERE user='foo';
user	user_attributes
foo	{"metadata": {"key": "value", "comment": "Added key/value pair"}, "Restrictions": [{"Database": "mysql", "Privileges": ["SELECT"]}], "Password_locking": {"failed_login_attempts": 4, "password_lock_time_days": 6}}
GRANT SELECT ON mysql.* TO foo@localhost;
# Partial revoke must be removed.
SELECT user, user_attributes FROM mysql.user WHERE user='foo';
user	user_attributes
foo	{"metadata": {"key": "value", "comment": "Added key/value pair"}, "Password_locking": {"failed_login_attempts": 4, "password_lock_time_days": 6}}
DROP USER foo@localhost;
