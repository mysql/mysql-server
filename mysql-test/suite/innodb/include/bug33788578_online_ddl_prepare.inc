# Prepare table according to requirement:
# b1 => index has no instant or version
# b2 => index has instant only
# b3 => index has version only
# b4 => index has both instant and version
#
# a1 => row has no instant or version
# a2 => row has instant only
# a3 => row has version only
# a4 => row has both instant and version

--echo # Processing alters:
--echo # b1, b2: no alter add in trunk
--echo # b3: alter add
--eval ALTER TABLE ta1b3_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta1b3_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--eval ALTER TABLE ta3b3_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta3b3_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--echo # b4: alter add
--eval ALTER TABLE ta1b4_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta1b4_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--eval ALTER TABLE ta2b4_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta2b4_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--eval ALTER TABLE ta3b4_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta3b4_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--echo # Processing row inserts:
--echo # a3: rows have version from new table
--eval INSERT INTO ta3b3_$row_format VALUES (1, "row1", 2, 3, 4, 5, 10, 11, 12, 13);
--eval INSERT INTO ta3b3_$row_format (c0, c1, c3, c5, c11, c13) VALUES (2, "row2", 33, 55, 1111, 1313);

--eval INSERT INTO ta3b4_$row_format VALUES (1, "row1", 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13);
--eval INSERT INTO ta3b4_$row_format (c0, c1, c3, c5, c7, c9, c11, c13) VALUES (2, "row2", 33, 55, 77, 99, 1111, 1313);
