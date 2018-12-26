SELECT @@GLOBAL.disconnect_on_expired_password;
@@GLOBAL.disconnect_on_expired_password
1
1 Expected
SET @@GLOBAL.disconnect_on_expired_password=0;
ERROR HY000: Variable 'disconnect_on_expired_password' is a read only variable
Expected error 'Read only variable'
SELECT @@GLOBAL.disconnect_on_expired_password;
@@GLOBAL.disconnect_on_expired_password
1
1 Expected
SELECT @@disconnect_on_expired_password = @@GLOBAL.disconnect_on_expired_password;
@@disconnect_on_expired_password = @@GLOBAL.disconnect_on_expired_password
1
1 Expected
SELECT COUNT(@@local.disconnect_on_expired_password);
ERROR HY000: Variable 'disconnect_on_expired_password' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.disconnect_on_expired_password);
ERROR HY000: Variable 'disconnect_on_expired_password' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT @@GLOBAL.disconnect_on_expired_password;
@@GLOBAL.disconnect_on_expired_password
1
1 Expected
