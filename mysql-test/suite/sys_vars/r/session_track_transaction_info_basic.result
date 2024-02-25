SELECT @@session.session_track_transaction_info INTO @old_track_tx;
SELECT @@global.session_track_transaction_info  INTO @old_track_tx_gbl;
SELECT @@global.session_track_transaction_info;
@@global.session_track_transaction_info
OFF
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
OFF
SET SESSION session_track_transaction_info=0;
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
OFF
SET SESSION session_track_transaction_info=1;
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
STATE
SET SESSION session_track_transaction_info=2;
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
CHARACTERISTICS
SET SESSION session_track_transaction_info=DEFAULT;
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
OFF
SET SESSION session_track_transaction_info=1.1;
ERROR 42000: Incorrect argument type to variable 'session_track_transaction_info'
SET SESSION session_track_transaction_info='OFF';
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
OFF
SET SESSION session_track_transaction_info='on';
ERROR 42000: Variable 'session_track_transaction_info' can't be set to the value of 'on'
SET SESSION session_track_transaction_info='STATE';
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
STATE
SET SESSION session_track_transaction_info='CHARACTERISTICS';
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
CHARACTERISTICS
SET SESSION session_track_transaction_info='OFF';
SET GLOBAL  session_track_transaction_info='STATE';
SET         session_track_transaction_info='CHARACTERISTICS';
SELECT @@global.session_track_transaction_info;
@@global.session_track_transaction_info
STATE
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
CHARACTERISTICS
SELECT @@session_track_transaction_info;
@@session_track_transaction_info
CHARACTERISTICS
SET GLOBAL  session_track_transaction_info='OFF';
SET         session_track_transaction_info='CHARACTERISTICS';
SET SESSION session_track_transaction_info='STATE';
SELECT @@global.session_track_transaction_info;
@@global.session_track_transaction_info
OFF
SELECT @@session.session_track_transaction_info;
@@session.session_track_transaction_info
STATE
SELECT @@session_track_transaction_info;
@@session_track_transaction_info
STATE
SET SESSION session_track_transaction_info=@old_track_tx;
SET GLOBAL  session_track_transaction_info=@old_track_tx_gbl;
