select collation(cast(0xA1A1 as char));
select hex(weight_string(cast(0x6141 as char)));
select hex(weight_string(cast(0x8EA1 as char)));
select hex(weight_string(cast(0x8EA1 as char) as char(1)));
select hex(weight_string(cast(0x8EA18EA1 as char) as char(1)));
select hex(weight_string(cast(0x8EA1 as char) as char(3)));
select hex(weight_string(cast(0x8EA18EA1 as char) as char(3)));
select hex(weight_string(cast(0x408EA1 as char) as char(3)));
select hex(weight_string(cast(0x408EA18EA1 as char) as char(3)));
select hex(weight_string(cast(0x408EA18EA18EA1 as char) as char(3)));
select hex(weight_string(cast(0x40408EA18EA18EA1 as char) as char(3)));

# Read 2 characters from the source string (the last character is not used)
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 1, 2, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 2, 2, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 3, 2, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 4, 2, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 5, 2, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char),25, 2, 0xC0));

# Read 3 characters from the source string (the entire string is used)
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 1, 3, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 2, 3, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 3, 3, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 4, 3, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 5, 3, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char),25, 3, 0xC0));

# Read 4 characters from the source string (extra space is added)
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 1, 4, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 2, 4, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 3, 4, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 4, 4, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char), 5, 4, 0xC0));
select hex(weight_string(cast(0x8EA18EA18EA1 as char),25, 4, 0xC0));

