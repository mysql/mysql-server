CALL mtr.add_suppression("Statement is unsafe because it is being used inside a XA transaction");
CREATE TABLE t1(f1 int);
XA START'','';
INSERT INTO t1 VALUES(10);
BINLOG '
SOgWTg8BAAAAbgAAAHIAAAAAAAQANS42LjMtbTUtZGVidWctbG9nAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAABI6BZOEzgNAAgAEgAEBAQEEgAAVgAEGggAAAAICAgCAAAAAAVAYI8=';
ERROR HY000: The system variable gtid_next cannot be set when there is an ongoing transaction.
XA END'';
XA PREPARE'';
XA COMMIT'';
SET @sav.pseudo_replica_mode= @@session.pseudo_replica_mode;
SELECT  @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
SET @@session.pseudo_replica_mode=1;
Warnings:
Warning	1231	'pseudo_replica_mode' is already ON.
BINLOG '
SOgWTg8BAAAAbgAAAHIAAAAAAAQANS42LjMtbTUtZGVidWctbG9nAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAABI6BZOEzgNAAgAEgAEBAQEEgAAVgAEGggAAAAICAgCAAAAAAVAYI8=';
SET @@gtid_next = "AUTOMATIC";
XA START 'xa_rollback';
INSERT INTO t1 VALUES(10);
XA END 'xa_rollback';
XA ROLLBACK 'xa_rollback';
SET @@session.pseudo_replica_mode=1;
BINLOG '
SOgWTg8BAAAAbgAAAHIAAAAAAAQANS42LjMtbTUtZGVidWctbG9nAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAABI6BZOEzgNAAgAEgAEBAQEEgAAVgAEGggAAAAICAgCAAAAAAVAYI8=';
SET @@gtid_next = "AUTOMATIC";
XA START 'xa_disconnect';
INSERT INTO t1 VALUES(10);
XA START 'xa_commit';
INSERT INTO t1 VALUES(10);
XA END 'xa_commit';
XA PREPARE 'xa_commit';
XA COMMIT 'xa_commit';
SET @@session.pseudo_replica_mode= @sav.pseudo_replica_mode;
DROP TABLE t1;
RESET MASTER;
CREATE TABLE t1 (c1 INT);
XA BEGIN 'xa1';
INSERT INTO t1 VALUES (1);
XA END 'xa1';
XA COMMIT 'xa1' ONE PHASE;
[Validate that SHOW BINLOG EVENTS shows the ONE PHASE statement.]
DROP TABLE t1;
RESET MASTER;
