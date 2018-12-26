--sorted_result
SELECT user,plugin,authentication_string FROM mysql.user WHERE user NOT IN ('root', 'mysql.sys', 'mysql.session');
