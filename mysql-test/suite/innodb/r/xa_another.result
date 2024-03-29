# The specific purpose of this test is to test the 
# detach at disconnect behavior
SET @saved_xa_detach_on_prepare = @@GLOBAL.xa_detach_on_prepare;
SET GLOBAL xa_detach_on_prepare = OFF;
CREATE TABLE t (id INT PRIMARY KEY);
XA START "xa1";
INSERT INTO t VALUES (1);
XA END "xa1";
XA PREPARE "xa1";
XA COMMIT "xa1";
ERROR XAE04: XAER_NOTA: Unknown XID
XA ROLLBACK "xa1";
ERROR XAE04: XAER_NOTA: Unknown XID
XA RECOVER;
formatID	gtrid_length	bqual_length	data
1	3	0	xa1
XA RECOVER;
formatID	gtrid_length	bqual_length	data
1	3	0	xa1
XA COMMIT "xa1";
XA RECOVER;
formatID	gtrid_length	bqual_length	data
XA COMMIT "xa1";
ERROR XAE04: XAER_NOTA: Unknown XID
DROP TABLE t;
SET GLOBAL xa_detach_on_prepare = @saved_xa_detach_on_prepare;
