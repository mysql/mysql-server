#
# Bug#28122841 - CREATE EVENT/PROCEDURE/FUNCTION CRASHES WITH ACCENT INSENSITIVE NAMES.
#                (This issue is observed with Resource groups too.)
#
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
#
# Test case to verify Resource group with case and accent insensitive names.
#
CREATE RESOURCE GROUP café TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;
# Resource group names are case and accent insensitive. So from the
# data-dictionary "cafe" is obtained for the following statement. Since
# MDL key comparison is case and accent sensitive, assert condition to verify
# expected lock with name "test.cafe" fails (lock is obtained on the
# test.café).
CREATE RESOURCE GROUP cafe TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;
ERROR HY000: Resource Group 'cafe' exists
# Following statement is to verify operation with the upper case name.
CREATE RESOURCE GROUP CAFE TYPE=USER VCPU=1-3 THREAD_PRIORITY=5;
ERROR HY000: Resource Group 'CAFE' exists
DROP RESOURCE GROUP CaFé;
SET NAMES default;
