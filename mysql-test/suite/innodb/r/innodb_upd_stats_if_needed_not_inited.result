CREATE TABLE parent (
a INT PRIMARY KEY
) ENGINE=INNODB;
CREATE TABLE child (
a INT PRIMARY KEY,
FOREIGN KEY (a) REFERENCES parent(a) ON DELETE CASCADE
) ENGINE=INNODB;
INSERT INTO parent VALUES (1);
INSERT INTO child VALUES (1);
# restart
SET SESSION DEBUG='+d,test_upd_stats_if_needed_not_inited';
DELETE FROM parent;
SET SESSION DEBUG='-d,test_upd_stats_if_needed_not_inited';
Pattern "test_upd_stats_if_needed_not_inited was executed" found
DROP TABLE child;
DROP TABLE parent;
