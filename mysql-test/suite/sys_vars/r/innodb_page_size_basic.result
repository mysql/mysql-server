SET @orig = @@global.innodb_page_size;
SELECT @orig;
@orig
{valid_page_size}
SET GLOBAL innodb_page_size = 4k;
ERROR HY000: Variable 'innodb_page_size' is a read only variable
SET GLOBAL innodb_page_size = 8k;
ERROR HY000: Variable 'innodb_page_size' is a read only variable
