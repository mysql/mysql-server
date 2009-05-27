-- connect (con1,localhost,root,,)
-- connect (con2,localhost,root,,)

-- connection con1
SET autocommit=0;
SELECT * FROM t1 FOR UPDATE;
-- if ($con1_extra_sql_present) {
	-- eval $con1_extra_sql
-- }

-- connection con2
SET autocommit=0;
SELECT * FROM t2 FOR UPDATE;
-- if ($con2_extra_sql_present) {
	-- eval $con2_extra_sql
-- }

-- if ($con1_should_be_rolledback) {
	-- connection con1
	-- send
	INSERT INTO t2 VALUES (0);

	-- connection con2
	INSERT INTO t1 VALUES (0);
	ROLLBACK;

	-- connection con1
	-- error ER_LOCK_DEADLOCK
	-- reap
-- }
# else
-- if (!$con1_should_be_rolledback) {
	-- connection con2
	-- send
	INSERT INTO t1 VALUES (0);

	-- connection con1
	INSERT INTO t2 VALUES (0);
	ROLLBACK;

	-- connection con2
	-- error ER_LOCK_DEADLOCK
	-- reap
-- }

-- connection default

DELETE FROM t5_nontrans;

-- disconnect con1
-- disconnect con2
