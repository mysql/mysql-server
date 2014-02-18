# The weight of 0xA2A9 will be the same with its capital letter 0xA2F9
select collation(cast(0xA2A9 as char));
select hex(weight_string(cast(0x6141 as char)));
select hex(weight_string(cast(0xA2A9 as char)));
select hex(weight_string(cast(0xA2A9 as char) as char(1)));
select hex(weight_string(cast(0xA2A9A2A9 as char) as char(1)));
select hex(weight_string(cast(0xA2A9 as char) as char(3)));
select hex(weight_string(cast(0xA2A9A2A9 as char) as char(3)));
select hex(weight_string(cast(0x40A2A9 as char) as char(3)));
select hex(weight_string(cast(0x40A2A9A2A9 as char) as char(3)));
select hex(weight_string(cast(0x40A2A9A2A9A2A9 as char) as char(3)));
select hex(weight_string(cast(0x4040A2A9A2A9A2A9 as char) as char(3)));

# Read 2 characters from the source string (the last character is not used)
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 1, 2, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 2, 2, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 3, 2, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 4, 2, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 5, 2, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char),25, 2, 0xC0));

# Read 3 characters from the source string (the entire string is used)
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 1, 3, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 2, 3, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 3, 3, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 4, 3, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 5, 3, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char),25, 3, 0xC0));

# Read 4 characters from the source string (extra space is added)
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 1, 4, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 2, 4, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 3, 4, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 4, 4, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char), 5, 4, 0xC0));
select hex(weight_string(cast(0xA2A9A2A9A2A9 as char),25, 4, 0xC0));

