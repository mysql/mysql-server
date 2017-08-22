#
# WL6378: New DD API Implementation
#         Test to compare collation of column_name and index_name on .FRM bases DD
#         and New DD.
#

set names latin1;
CREATE TABLE t1 (`a` INT, `b` INT, `C` INT,
                 `D` INT, `á` INT, `é` INT,
                 `Á` INT, `Å` INT, `ê` INT,
                 KEY `a` ( `a` ), KEY `b` ( `b` ), KEY `C` ( `C` ),
                 KEY `D` ( `D` ), KEY `á` ( `á` ), KEY `é` ( `é` ),
                 KEY `Á` ( `Á` ), KEY `Å` ( `Å` ), KEY `ê` ( `ê` ));

CREATE TABLE t2 (`ê` INT, `Å` INT, `Á` INT,
                 `é` INT, `á` INT, `D` INT,
                 `C` INT, `b` INT, `a` INT,
                 KEY `ê` ( `ê` ), KEY `Á` ( `Á` ), KEY `Å` ( `Å` ),
                 KEY `é` ( `é` ), KEY `á` ( `á` ), KEY `D` ( `D` ),
                 KEY `C` ( `C` ), KEY `b` ( `b` ), KEY `a` ( `a` ));

# Results of following queries are generated on .FRM based DD and compared with
# result of new DD.
# Pass Criteria: There should not be any result mismatch on new DD code.
SELECT table_name, column_name FROM INFORMATION_SCHEMA.COLUMNS WHERE table_name= 't1' ORDER BY column_name;
SELECT table_name, column_name FROM INFORMATION_SCHEMA.COLUMNS WHERE table_name= 't2' ORDER BY column_name;

SELECT table_name, index_name, column_name FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_NAME= 't1' ORDER BY index_name;
SELECT table_name, index_name, column_name FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_NAME= 't2' ORDER BY index_name;

# Cleanup
DROP TABLE t2, t1;
