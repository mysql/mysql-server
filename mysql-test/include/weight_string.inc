select @@collation_connection;
select hex(weight_string('a'));
select hex(weight_string('A'));
select hex(weight_string('abc'));
select hex(weight_string('abc' as char(2)));
select hex(weight_string('abc' as char(3)));
select hex(weight_string('abc' as char(5)));

# Read 2 characters from the source string (the last character is not used).
select hex(weight_string('abc', 1, 2, 0xC0));
select hex(weight_string('abc', 2, 2, 0xC0));
select hex(weight_string('abc', 3, 2, 0xC0));
select hex(weight_string('abc', 4, 2, 0xC0));
select hex(weight_string('abc', 5, 2, 0xC0));
select hex(weight_string('abc',25, 2, 0xC0));

# Read 3 characters from the source string (the entire string is used).
select hex(weight_string('abc', 1, 3, 0xC0));
select hex(weight_string('abc', 2, 3, 0xC0));
select hex(weight_string('abc', 3, 3, 0xC0));
select hex(weight_string('abc', 4, 3, 0xC0));
select hex(weight_string('abc', 5, 3, 0xC0));
select hex(weight_string('abc',25, 3, 0xC0));

# Read 4 characters from the source string (extra space is added)
select hex(weight_string('abc', 1, 4, 0xC0));
select hex(weight_string('abc', 2, 4, 0xC0));
select hex(weight_string('abc', 3, 4, 0xC0));
select hex(weight_string('abc', 4, 4, 0xC0));
select hex(weight_string('abc', 5, 4, 0xC0));
select hex(weight_string('abc',25, 4, 0xC0));
