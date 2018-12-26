
# Test default delimiter ;
select "Test default delimiter ;" as " ";
select * from t1;

# Test delimiter without argument
select "Test delimiter without arg" as " ";
# Nothing should be displayed, error is returned
delimiter
delimiter ; # Reset delimiter

# Test delimiter :
select "Test delimiter :" as " ";
delimiter :
select * from t1:
delimiter ; # Reset delimiter

# Test delimiter ':'
select "Test delimiter :" as " ";
delimiter ':'
select * from t1:
delimiter ; # Reset delimiter

# Test delimiter :;
select "Test delimiter :;" as " ";
delimiter :;
select * from t1 :;
delimiter ; # Reset delimiter

## Test delimiter //
select "Test delimiter //" as " ";
delimiter //
select * from t1//
delimiter ; # Reset delimiter

# Test delimiter 'MySQL'
select "Test delimiter MySQL" as " ";
delimiter 'MySQL'
select * from t1MySQL
delimiter ; # Reset delimiter

# Test delimiter 'delimiter'(should be allowed according to the code)
select "Test delimiter delimiter" as " ";
delimiter delimiter
select * from t1 delimiter
delimiter ; # Reset delimiter

#
# Bug #11523: \d works differently than delimiter
#
source t/mysql_delimiter_source.sql
delimiter ; # Reset delimiter

#
# Bug #19799: delimiter command not working correctly when sourcing a sql file
#             with Windows style line endings.
#
source t/mysql_delimiter_19799.sql
use test//
show tables//
delimiter ; # Reset delimiter

#
# Bug #36244: MySQL CLI doesn't recognize standalone -- as comment
#             before DELIMITER statement
#
--
DELIMITER ;
