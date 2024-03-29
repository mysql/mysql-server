--echo # Insert some rows just after upgrade
--echo # a1 => no alter in 8.0.28
--echo #    ta1b1_*, ta1b2_*, ta1b4_* -- insert rows

--eval INSERT INTO ta1b1_$row_format VALUES (11, "row11", 12, 13, 14, 15);
--eval INSERT INTO ta1b1_$row_format (c0, c1, c3, c5) VALUES (12, "row12", 133, 155);

--eval INSERT INTO ta1b2_$row_format VALUES (11, "row11", 12, 13, 14, 15);
--eval INSERT INTO ta1b2_$row_format (c0, c1, c3, c5) VALUES (12, "row12", 133, 155);

--eval INSERT INTO ta1b4_$row_format VALUES (11, "row11", 12, 13, 14, 15);
--eval INSERT INTO ta1b4_$row_format (c0, c1, c3, c5) VALUES (12, "row12", 133, 155);

--echo # a2 => alter in 8.0.28 but no insert
--echo #    ta2b1_*, ta2b2_*, ta2b4_* -- insert rows:

--eval INSERT INTO ta2b1_$row_format VALUES (13, "row13", 1222, 1333, 1444, 1555, 1666, 1777, 1888, 1999);
--eval INSERT INTO ta2b1_$row_format (c0, c1, c3, c5, c7, c9) VALUES (14, "row14", 13333, 15555, 17777, 19999);

--eval INSERT INTO ta2b2_$row_format VALUES (13, "row13", 1222, 1333, 1444, 1555, 1666, 1777, 1888, 1999);
--eval INSERT INTO ta2b2_$row_format (c0, c1, c3, c5, c7, c9) VALUES (14, "row14", 13333, 15555, 17777, 19999);

--eval INSERT INTO ta2b4_$row_format VALUES (13, "row13", 1222, 1333, 1444, 1555, 1666, 1777, 1888, 1999);
--eval INSERT INTO ta2b4_$row_format (c0, c1, c3, c5, c7, c9) VALUES (14, "row14", 13333, 15555, 17777, 19999);

--echo # a4 => alter + insert in 8.0.28
--echo #    ta4b1_*, ta4b2_*, ta4b4_* -- insert rows

--eval INSERT INTO ta4b1_$row_format VALUES (13, "row13", 1222, 1333, 1444, 1555, 1666, 1777, 1888, 1999);
--eval INSERT INTO ta4b1_$row_format (c0, c1, c3, c5, c7, c9) VALUES (14, "row14", 13333, 15555, 17777, 19999);

--eval INSERT INTO ta4b2_$row_format VALUES (13, "row13", 1222, 1333, 1444, 1555, 1666, 1777, 1888, 1999);
--eval INSERT INTO ta4b2_$row_format (c0, c1, c3, c5, c7, c9) VALUES (14, "row14", 13333, 15555, 17777, 19999);

--eval INSERT INTO ta4b4_$row_format VALUES (13, "row13", 1222, 1333, 1444, 1555, 1666, 1777, 1888, 1999);
--eval INSERT INTO ta4b4_$row_format (c0, c1, c3, c5, c7, c9) VALUES (14, "row14", 13333, 15555, 17777, 19999);

--echo # b1 => no alter in trunk
--echo #    ta1b1_*, ta2b1_*, ta4b1_* -- no operation
--echo # b2 => alter in trunk but no insert
--echo #    ta1b2_*, ta2b2_*, ta4b1_* -- ALTER TABLE no insert:

--eval ALTER TABLE ta1b2_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta1b2_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--eval ALTER TABLE ta2b2_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta2b2_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--eval ALTER TABLE ta4b2_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta4b2_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--echo # b4 => alter + insert in trunk
--echo #    ta1b4_*, ta2b4_*, ta4b4_* -- ALTER TABLE + INSERT INTO

--eval ALTER TABLE ta1b4_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta1b4_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--eval INSERT INTO ta1b4_$row_format VALUES (5, "row5", 2002, 3003, 4004, 5005, 1010, 1111, 1212, 1313);
--eval INSERT INTO ta1b4_$row_format (c0, c1, c3, c5, c11, c13) VALUES (6, "row6", 30030, 50050, 11110, 13130);

--eval ALTER TABLE ta2b4_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta2b4_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--eval INSERT INTO ta2b4_$row_format VALUES (5, "row5", 2002, 3003, 4004, 5005, 6006, 7007, 8008, 9009, 1010, 1111, 1212, 1313);
--eval INSERT INTO ta2b4_$row_format (c0, c1, c3, c5, c7, c9, c11, c13) VALUES (6, "row6", 30030, 50050, 70070, 90090, 11110, 13130);

--eval ALTER TABLE ta4b4_$row_format ADD COLUMN c10 INT, ADD COLUMN c11 INT NOT NULL, ALGORITHM=INSTANT;
--eval ALTER TABLE ta4b4_$row_format ADD COLUMN c12 INT DEFAULT 120, ADD COLUMN c13 INT NOT NULL DEFAULT 130, ALGORITHM=INSTANT;

--eval INSERT INTO ta4b4_$row_format VALUES (5, "row5", 2002, 3003, 4004, 5005, 6006, 7007, 8008, 9009, 1010, 1111, 1212, 1313);
--eval INSERT INTO ta4b4_$row_format (c0, c1, c3, c5, c7, c9, c11, c13) VALUES (6, "row6", 30030, 50050, 70070, 90090, 11110, 13130);

