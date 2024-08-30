--source include/not_mac_os.inc

--echo Resource group hints

CREATE TABLE t1 (
    f1 int unsigned NOT NULL auto_increment primary key,
    f2 varchar(100) NOT NULL default ''
);
INSERT INTO t1 values(1, 'resource_grp');

SELECT /*+ RESOURCE_GROUP(notexist) */ * from t1;
--replace_regex /thread id \([0-9]+\)/thread id ID/
SELECT /*+ RESOURCE_GROUP(SYS_default) */ * from t1;

DROP TABLE t1;
