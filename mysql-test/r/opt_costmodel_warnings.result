SELECT * FROM mysql.server_cost;
cost_name	cost_value	last_update	comment	default_value
disk_temptable_create_cost	NULL	#	NULL	20
disk_temptable_row_cost	NULL	#	NULL	0.5
key_compare_cost	NULL	#	NULL	0.05
memory_temptable_create_cost	NULL	#	NULL	1
memory_temptable_row_cost	NULL	#	NULL	0.1
row_evaluate_cost	NULL	#	NULL	0.1
SELECT * FROM mysql.engine_cost;
engine_name	device_type	cost_name	cost_value	last_update	comment	default_value
default	0	io_block_read_cost	NULL	#	NULL	1
default	0	memory_block_read_cost	NULL	#	NULL	0.25
RENAME TABLE mysql.engine_cost TO mysql.engine_cost_renamed;
"Restarting MySQL server"
# restart
"MySQL restarted"
RENAME TABLE mysql.engine_cost_renamed TO mysql.engine_cost;
SELECT * FROM mysql.engine_cost;
engine_name	device_type	cost_name	cost_value	last_update	comment	default_value
default	0	io_block_read_cost	NULL	#	NULL	1
default	0	memory_block_read_cost	NULL	#	NULL	0.25
INSERT INTO mysql.server_cost
VALUES ("lunch_cost", 1.0, CURRENT_TIMESTAMP, "Lunch is important", DEFAULT);
UPDATE mysql.server_cost
SET cost_value=-1.0
WHERE cost_name="row_evaluate_cost";
UPDATE mysql.server_cost
SET cost_value=0.0
WHERE cost_name="key_compare_cost";
"Restarting MySQL server"
# restart
"MySQL restarted"
DELETE FROM mysql.server_cost
WHERE cost_name LIKE "lunch_cost%";
UPDATE mysql.server_cost
SET cost_value=NULL
WHERE cost_name="row_evaluate_cost";
UPDATE mysql.server_cost
SET cost_value=NULL
WHERE cost_name="key_compare_cost";
INSERT INTO mysql.engine_cost
VALUES ("InnoDB", 0, "lunch_cost", 1.0, CURRENT_TIMESTAMP, "Lunch is important", DEFAULT);
UPDATE mysql.engine_cost
SET cost_value=0.0
WHERE cost_name="io_block_read_cost";
INSERT INTO mysql.engine_cost
VALUES ("Falcon", 0, "io_block_read_cost", 1.0, CURRENT_TIMESTAMP, "Unknown storage engine", DEFAULT);
INSERT INTO mysql.engine_cost
VALUES ("InnoDB", -1, "io_block_read_cost", 1.0, CURRENT_TIMESTAMP, "1 is an illegal device type", DEFAULT);
"Restarting MySQL server"
# restart
"MySQL restarted"
DELETE FROM mysql.engine_cost
WHERE cost_name LIKE "lunch_cost%";
UPDATE mysql.engine_cost
SET cost_value=NULL;
DELETE FROM mysql.engine_cost
WHERE device_type = -1;
DELETE FROM mysql.engine_cost
WHERE engine_name LIKE "Falcon";
SELECT * FROM mysql.server_cost;
cost_name	cost_value	last_update	comment	default_value
disk_temptable_create_cost	NULL	#	NULL	20
disk_temptable_row_cost	NULL	#	NULL	0.5
key_compare_cost	NULL	#	NULL	0.05
memory_temptable_create_cost	NULL	#	NULL	1
memory_temptable_row_cost	NULL	#	NULL	0.1
row_evaluate_cost	NULL	#	NULL	0.1
SELECT * FROM mysql.engine_cost;
engine_name	device_type	cost_name	cost_value	last_update	comment	default_value
default	0	io_block_read_cost	NULL	#	NULL	1
default	0	memory_block_read_cost	NULL	#	NULL	0.25
