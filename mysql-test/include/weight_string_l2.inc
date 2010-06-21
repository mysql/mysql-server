select @@collation_connection;
select hex(weight_string('a' LEVEL 2));
select hex(weight_string('A' LEVEL 2));
select hex(weight_string('abc' LEVEL 2));
select hex(weight_string('abc' as char(2) LEVEL 2));
select hex(weight_string('abc' as char(3) LEVEL 2));
select hex(weight_string('abc' as char(5) LEVEL 2));
