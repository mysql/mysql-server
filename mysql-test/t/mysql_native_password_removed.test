--echo #
--echo # WL#15930 remove deprecated mysql_native_password authentication
--echo #

--error ER_PLUGIN_IS_NOT_LOADED
CREATE USER native_password_user IDENTIFIED WITH mysql_native_password BY 'abcd';

CREATE USER regular_user;

--error ER_PLUGIN_IS_NOT_LOADED
ALTER USER regular_user IDENTIFIED WITH mysql_native_password BY 'abcd';

DROP USER regular_user;
