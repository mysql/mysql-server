SELECT is_ipv4(inet_ntoa('1'));
SELECT is_ipv6(inet_ntoa('1'));
SELECT inet6_aton(inet_ntoa('1'));
SELECT inet6_ntoa(inet_ntoa('1'));

--echo #
--echo # Bug#14040277 UNINITIALIZED VALUE REFERENCED IN STR_TO_IPV6
--echo #
SELECT inet6_aton(soundex('a'));
