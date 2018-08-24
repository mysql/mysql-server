#
# Bug#20315088 LCASE/LTRIM, SOURCE AND DESTINATION OVERLAP IN MEMCPY
#
do lcase(ltrim(from_unixtime(0,' %T ')));
do _cp852 "" <= lcase(trim(leading 1 from 12222)) not between '1' and '2';
do upper(substring(1.111111111111111111 from '2n'));
Warnings:
Warning	1292	Truncated incorrect INTEGER value: '2n'
Warning	1292	Truncated incorrect INTEGER value: '2n'
do nullif(1,'-' between lcase(right(11111111," 7,]" ))and '1');
Warnings:
Warning	1292	Truncated incorrect INTEGER value: ' 7,]'
Warning	1292	Truncated incorrect INTEGER value: ' 7,]'
do upper(right(198039009115594390000000000000000000000.000000,35));
do concat('111','11111111111111111111111111',
substring_index(uuid(),0,1.111111e+308));
do replace(ltrim(from_unixtime(0,' %T ')), '0', '1');
do insert(ltrim(from_unixtime(0,' %T ')), 2, 1, 'hi');
set @old_collation_connection=@@collation_connection;
set collation_connection="utf8_general_ci";
Warnings:
Warning	3778	'utf8_general_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
do replace(ltrim(from_unixtime(0,' %T ')), '0', '1');
set collation_connection=@old_collation_connection;
