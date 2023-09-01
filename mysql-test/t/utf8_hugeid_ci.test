--echo #
--echo # Verify that we reject utf8_hugeid_ci
--echo #

call mtr.add_suppression("Too big collation id");
call mtr.add_suppression("Error while parsing");

--error ER_UNKNOWN_COLLATION
CREATE TABLE t1 (a VARCHAR(10)) COLLATE utf8mb3_hugeid_ci;
