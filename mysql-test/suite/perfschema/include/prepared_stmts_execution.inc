# 
# Execution of all the prepared statements created in 
# prepared_statements_setup.inc.
# 

SET @a = 3;
SET @b = 4;
EXECUTE st1 USING @a, @b;

#SET @table = 't1';
EXECUTE st2;

SET @c=3;
EXECUTE st3 using @c;

EXECUTE st4;


