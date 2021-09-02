# Should be off by default
SELECT @@GLOBAL.print_identified_with_as_hex, @@SESSION.print_identified_with_as_hex;
@@GLOBAL.print_identified_with_as_hex	@@SESSION.print_identified_with_as_hex
0	0
SET GLOBAL print_identified_with_as_hex=1;
SELECT @@GLOBAL.print_identified_with_as_hex, @@SESSION.print_identified_with_as_hex;
@@GLOBAL.print_identified_with_as_hex	@@SESSION.print_identified_with_as_hex
1	0
SET SESSION print_identified_with_as_hex=1;
SELECT @GLOBAL.print_identified_with_as_hex, @@SESSION.print_identified_with_as_hex;
@GLOBAL.print_identified_with_as_hex	@@SESSION.print_identified_with_as_hex
NULL	1
# This corresponds to CREATE USER foo@localhost IDENTIFIED BY 'bar'
# But because of the salt we peg the whole hash instead.
CREATE USER 'foo'@'localhost' IDENTIFIED WITH 'caching_sha2_password' AS 0x244124303035240C4D7A6D25436F2C0A08515310644615383E2A123961484C6276734178425A446172436B58446A582F6178544A692E6F644E4F2F4E596E666276454B563336 REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT;
CREATE USER oldfoo@localhost IDENTIFIED WITH 'mysql_native_password' BY 'bar';
# Should display hex: unprintable symbols
SHOW CREATE USER foo@localhost;
CREATE USER for foo@localhost
CREATE USER `foo`@`localhost` IDENTIFIED WITH 'caching_sha2_password' AS 0x244124303035240C4D7A6D25436F2C0A08515310644615383E2A123961484C6276734178425A446172436B58446A582F6178544A692E6F644E4F2F4E596E666276454B563336 REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT
# Should not display hex: native passwords not having non-printable bytes
SHOW CREATE USER oldfoo@localhost;
CREATE USER for oldfoo@localhost
CREATE USER `oldfoo`@`localhost` IDENTIFIED WITH 'mysql_native_password' AS '*E8D46CE25265E545D225A8A6F1BAF642FEBEE5CB' REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT
SET SESSION print_identified_with_as_hex=0;
# Should not display hex: turned off
SHOW CREATE USER foo@localhost;
CREATE USER for foo@localhost
CREATE USER `foo`@`localhost` IDENTIFIED WITH 'caching_sha2_password' AS '$A$005$Mzm%Co,\nQSdF8>*9aHLbvsAxBZDarCkXDjX/axTJi.odNO/NYnfbvEKV36' REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT
# Should not display hex: turned off
SHOW CREATE USER oldfoo@localhost;
CREATE USER for oldfoo@localhost
CREATE USER `oldfoo`@`localhost` IDENTIFIED WITH 'mysql_native_password' AS '*E8D46CE25265E545D225A8A6F1BAF642FEBEE5CB' REQUIRE NONE PASSWORD EXPIRE DEFAULT ACCOUNT UNLOCK PASSWORD HISTORY DEFAULT PASSWORD REUSE INTERVAL DEFAULT PASSWORD REQUIRE CURRENT DEFAULT
# Now try to reply the statement produced by SHOW CREATE
DROP USER foo@localhost;
# Replaying the collected statement
# Connecting as foo: should work
SELECT 1;
1
1
# back to default connection
# Cleanup
DROP USER foo@localhost,oldfoo@localhost;
SET GLOBAL print_identified_with_as_hex=default;
