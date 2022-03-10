SET SESSION innodb_ddl_threads=1;
SET SESSION innodb_parallel_read_threads=1;
CREATE TABLE wl5889 (a INT) ENGINE=INNODB;
INSERT INTO wl5889 VALUE(1);
ALTER TABLE wl5889 ADD COLUMN b INT, ALGORITHM=INPLACE;
SELECT
event_name,
work_completed > 0,
work_estimated > 0,
work_completed <= work_estimated
FROM performance_schema.events_stages_history
WHERE event_name LIKE 'stage/innodb/alter table%'
ORDER BY 1;
event_name	stage/innodb/alter table (end)
work_completed > 0	1
work_estimated > 0	1
work_completed <= work_estimated	1
event_name	stage/innodb/alter table (end)
work_completed > 0	0
work_estimated > 0	0
work_completed <= work_estimated	1
event_name	stage/innodb/alter table (end)
work_completed > 0	1
work_estimated > 0	1
work_completed <= work_estimated	1
event_name	stage/innodb/alter table (log apply table)
work_completed > 0	1
work_estimated > 0	1
work_completed <= work_estimated	1
event_name	stage/innodb/alter table (log apply table)
work_completed > 0	1
work_estimated > 0	1
work_completed <= work_estimated	1
event_name	stage/innodb/alter table (read PK and internal sort)
work_completed > 0	1
work_estimated > 0	1
work_completed <= work_estimated	1
event_name	stage/innodb/alter table (read PK and internal sort)
work_completed > 0	0
work_estimated > 0	0
work_completed <= work_estimated	1
DROP TABLE wl5889;
