# Can't run with embedded server.
--source include/not_embedded.inc

--echo # Verify that the client can show JSON data and metadata.

CREATE TABLE t(i INT, j1 JSON, j2 JSON NOT NULL);
SHOW CREATE TABLE t;
INSERT INTO t VALUES
(0, NULL, 'null'),
(1, '1', '1'),
(2, '[1,2,3]', '[4,5,6]'),
(3, '{"abc":"def"}', '{"a":"b","c":"d"}');

# Filter out unstable timing information for the query.
--replace_regex /\([0-9]+\.[0-9]+ sec\)/<FILTERED>/

# Execute the client. Enable printing of column metadata.
--exec $MYSQL -vvv --column-type-info test -e "SELECT j1, j2 FROM t ORDER BY i"

DROP TABLE t;
