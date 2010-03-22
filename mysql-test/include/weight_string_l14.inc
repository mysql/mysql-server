select @@collation_connection;
select hex(weight_string('a' LEVEL 1,3));
select hex(weight_string('a' LEVEL 1-3));
select hex(weight_string('A' LEVEL 1,3));
select hex(weight_string('A' LEVEL 1-3));

select hex(weight_string('a' LEVEL 1,4));
select hex(weight_string('a' LEVEL 1-4));
select hex(weight_string('A' LEVEL 1,4));
select hex(weight_string('A' LEVEL 1-4));

select hex(weight_string('a' LEVEL 2,3));
select hex(weight_string('a' LEVEL 2-3));
select hex(weight_string('A' LEVEL 2,3));
select hex(weight_string('A' LEVEL 2-3));

select hex(weight_string('a' LEVEL 2,4));
select hex(weight_string('a' LEVEL 2-4));
select hex(weight_string('A' LEVEL 2,4));
select hex(weight_string('A' LEVEL 2-4));

select hex(weight_string('a' LEVEL 3,4));
select hex(weight_string('a' LEVEL 3-4));
select hex(weight_string('A' LEVEL 3,4));
select hex(weight_string('A' LEVEL 3-4));

select hex(weight_string('a' LEVEL 1,2,3,4));
select hex(weight_string('a' LEVEL 2,3,4));
select hex(weight_string('a' LEVEL 1,3,4));
select hex(weight_string('a' LEVEL 1,2,3));

# If any number is less than 1, it is treated as 1. 
select hex(weight_string('a' LEVEL 0));

#If any number is greater than the maximum, it is treated as the maximum. 
select hex(weight_string('a' LEVEL 8));
select hex(weight_string('a' LEVEL 1,8));
