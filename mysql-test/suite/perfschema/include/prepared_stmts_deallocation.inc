# 
# Deallocates all the prepared statements 
# created in prepares_stmts_setup.inc
#

DEALLOCATE PREPARE st1;
DEALLOCATE PREPARE st2;
DEALLOCATE PREPARE st3;
DEALLOCATE PREPARE st4;

DROP TABLE t1; 
