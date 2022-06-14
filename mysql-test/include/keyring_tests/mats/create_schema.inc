## Create test schema
CREATE TABLE t1(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB);

CREATE TABLE t2(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
PARTITION BY KEY(col1) PARTITIONS 5;

DELIMITER |;

# Procedure to execute dml commands in loop
#	p_dml_type	[IN]	DML type
#		0 -> INSERT
#		1 -> UPDATE
#		2 -> DELETE
#	p_key_min	[IN]	Minimum key value
#	p_key_range	[IN]	Maximum key range
#	p_loop_count	[IN]	Number of times to execute
#	p_frequency	[IN]	Frequency of commit/rollback
#	p_is_rand	[IN]	chose key randomly and do rollback and commit

CREATE PROCEDURE execute_dml(
	p_dml_type	INT,
	p_key_min	INT,
	p_key_range	INT,
	p_loop_count	INT,
	p_frequency	INT,
	p_is_rand	INT)
BEGIN
	DECLARE v_idx		INT DEFAULT 0;
	DECLARE v_commit	INT DEFAULT 0;
	DECLARE v_key		INT DEFAULT 0;

	/* Loop and INSERT data at random position */
	WHILE(v_idx < p_loop_count) DO

		/* Generate key between 1 to p_loop_count */
		IF p_is_rand = 1 THEN
			SET v_key = p_key_min + FLOOR(RAND() * p_key_range);
		ELSE
			SET v_key = p_key_min + (v_idx  % p_key_range);
		END IF;

		CASE p_dml_type

			WHEN 0 THEN
				SET @clol3_text = CONCAT('Clone Test Row - ', v_key);
				INSERT INTO t1 VALUES(v_key, v_key * 10,
					@clol3_text, REPEAT('Large Column Data ', 2048))
					ON DUPLICATE KEY UPDATE col2 = col2 + 1;

				INSERT INTO t2 VALUES(v_key, v_key * 10,
					@clol3_text, REPEAT('Large Column Data ', 2048))
					ON DUPLICATE KEY UPDATE col2 = col2 + 1;

			WHEN 1 THEN
				UPDATE t1 SET col2 = v_idx + 1 WHERE col1 = v_key;
				UPDATE t2 SET col2 = v_idx + 1 WHERE col1 = v_key;

			WHEN 2 THEN
				DELETE FROM t1 WHERE col1 = v_key;
				DELETE FROM t2 WHERE col1 = v_key;

			ELSE
				DELETE FROM t1;
				DELETE FROM t2;
		END CASE;

		SET v_idx = v_idx + 1;

		/* Commit or rollback work at specified frequency. */
		IF v_idx % p_frequency = 0 THEN

			SET v_commit = FLOOR(RAND() * 2);
			IF v_commit = 0 AND p_is_rand = 1 THEN
				ROLLBACK;
				START TRANSACTION;
			ELSE
				COMMIT;
				START TRANSACTION;
			END IF;
		END IF;

	END WHILE;
END|

DELIMITER ;|
