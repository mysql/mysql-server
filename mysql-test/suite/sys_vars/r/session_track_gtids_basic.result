include/assert.inc [Assert that the global default value is OFF.]
include/assert.inc [Assert that the session default value is OFF.]
SET @@GLOBAL.session_track_gtids=ALL_GTIDS;
SET @@SESSION.session_track_gtids=ALL_GTIDS;
include/assert.inc [Assert that the global value is ALL_GTIDS]
include/assert.inc [Assert that the session value is ALL_GTIDS]
include/assert.inc [Assert that I_S.GLOBAL_VARIABLES captures the variable value.]
include/assert.inc [Assert that I_S.SESSION_VARIABLES captures the variable value.]
SET @@GLOBAL.session_track_gtids=OWN_GTID;
SET @@SESSION.session_track_gtids=OWN_GTID;
include/assert.inc [Assert that the global value is OWN_GTID]
include/assert.inc [Assert that the session value is OWN_GTID]
include/assert.inc [Assert that I_S.GLOBAL_VARIABLES captures the variable value.]
include/assert.inc [Assert that I_S.SESSION_VARIABLES captures the variable value.]
SET @@GLOBAL.session_track_gtids=OFF;
SET @@SESSION.session_track_gtids=OFF;
include/assert.inc [Assert that the global value is OFF]
include/assert.inc [Assert that the session value is OFF]
include/assert.inc [Assert that I_S.GLOBAL_VARIABLES captures the variable value.]
include/assert.inc [Assert that I_S.SESSION_VARIABLES captures the variable value.]
include/assert.inc [Assert that COUNT(@@session_track_gtids)=1]
include/assert.inc [Assert that COUNT(@@local.session_track_gtids)=1]
include/assert.inc [Assert that COUNT(@@global.session_track_gtids)=1]
include/assert.inc [Assert that COUNT(@@session.session_track_gtids)=1]
SET @@GLOBAL.session_track_gtids=DEFAULT;
include/assert.inc [Assert that the global default value is OFF]
SET @@SESSION.session_track_gtids=DEFAULT;
include/assert.inc [Assert that the session default value is OFF]
BEGIN;
SET @@SESSION.session_track_gtids=OFF;
ERROR HY000: The system variable session_track_gtids cannot be set when there is an ongoing transaction.
ROLLBACK;
