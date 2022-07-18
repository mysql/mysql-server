--echo #
--echo # Bug #34303841: Inappropriate error handling for max_allowed_packet
--echo #

# init
SET GLOBAL max_allowed_packet=16777216;
CREATE TABLE b34303841(id int key auto_increment, id2 longtext);
INSERT INTO b34303841 (id2) VALUES (repeat('a',16777216));

let $cmd_file= $MYSQL_TMP_DIR/b34303841.sql;
write_file $cmd_file;
SELECT * FROM b34303841;
SELECT 12;
EOF

# test : must return max packet error and then a 12.

--exec $MYSQL test -f -v 2>&1 < $cmd_file

#cleanup
remove_file $cmd_file;
DROP TABLE b34303841;
SET GLOBAL max_allowed_packet=default;


--echo # End of 8.0 tests
