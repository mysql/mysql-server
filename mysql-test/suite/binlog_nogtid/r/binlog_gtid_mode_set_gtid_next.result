include/rpl_init.inc [topology=none]
SET GLOBAL ENFORCE_GTID_CONSISTENCY = ON;
==== GTID_MODE = OFF ====
---- Old value is AUTOMATIC or ANONYMOUS ----
# AUTOMATIC -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
Expecting error ER_CANT_SET_GTID_NEXT_TO_GTID_WHEN_GTID_MODE_IS_OFF.
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
ERROR HY000: @@SESSION.GTID_NEXT cannot be set to UUID:NUMBER when @@GLOBAL.GTID_MODE = OFF.
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
# AUTOMATIC -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
# ANONYMOUS -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
Expecting error ER_CANT_SET_GTID_NEXT_TO_GTID_WHEN_GTID_MODE_IS_OFF.
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
ERROR HY000: @@SESSION.GTID_NEXT cannot be set to UUID:NUMBER when @@GLOBAL.GTID_MODE = OFF.
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == ""]
# ANONYMOUS -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
# ANONYMOUS -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
# AUTOMATIC -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
---- Old value is UUID:NUMBER ----
SET GLOBAL GTID_MODE = 'OFF_PERMISSIVE';
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';;
ROLLBACK;
SET GLOBAL GTID_MODE = 'OFF';
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
Expecting error ER_CANT_SET_GTID_NEXT_TO_GTID_WHEN_GTID_MODE_IS_OFF.
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
ERROR HY000: @@SESSION.GTID_NEXT cannot be set to UUID:NUMBER when @@GLOBAL.GTID_MODE = OFF.
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == ""]
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2
Expecting error ER_CANT_SET_GTID_NEXT_TO_GTID_WHEN_GTID_MODE_IS_OFF.
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
ERROR HY000: @@SESSION.GTID_NEXT cannot be set to UUID:NUMBER when @@GLOBAL.GTID_MODE = OFF.
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == ""]
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
SET GLOBAL GTID_MODE = 'OFF_PERMISSIVE';
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';;
ROLLBACK;
SET GLOBAL GTID_MODE = 'OFF';
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
==== GTID_MODE = OFF_PERMISSIVE ====
SET GLOBAL GTID_MODE = 'OFF_PERMISSIVE';
# AUTOMATIC -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
# AUTOMATIC -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
# ANONYMOUS -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
# ANONYMOUS -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2 -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
# AUTOMATIC -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
# ANONYMOUS -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
==== GTID_MODE = ON_PERMISSIVE ====
SET GLOBAL GTID_MODE = ON_PERMISSIVE;
# AUTOMATIC -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
# AUTOMATIC -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
# ANONYMOUS -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
# ANONYMOUS -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2 -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
# AUTOMATIC -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == "ANONYMOUS"]
ROLLBACK;
# ANONYMOUS -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
==== GTID_MODE = ON ====
---- Old value is AUTOMATIC or GTID ----
SET @@GLOBAL.GTID_MODE = ON;
# AUTOMATIC -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
# AUTOMATIC -> ANONYMOUS
Expecting error ER_CANT_SET_GTID_NEXT_TO_ANONYMOUS_WHEN_GTID_MODE_IS_ON.
SET GTID_NEXT = 'ANONYMOUS';
ERROR HY000: @@SESSION.GTID_NEXT cannot be set to ANONYMOUS when @@GLOBAL.GTID_MODE = ON.
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
# AUTOMATIC -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2 -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
# AUTOMATIC -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> ANONYMOUS
Expecting error ER_CANT_SET_GTID_NEXT_TO_ANONYMOUS_WHEN_GTID_MODE_IS_ON.
SET GTID_NEXT = 'ANONYMOUS';
ERROR HY000: @@SESSION.GTID_NEXT cannot be set to ANONYMOUS when @@GLOBAL.GTID_MODE = ON.
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == ""]
# aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1 -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
---- Old value is ANONYMOUS ----
SET GLOBAL GTID_MODE = 'ON_PERMISSIVE';
SET GTID_NEXT = 'ANONYMOUS';;
ROLLBACK;
SET GLOBAL GTID_MODE = 'ON';
# ANONYMOUS -> ANONYMOUS
Expecting error ER_CANT_SET_GTID_NEXT_TO_ANONYMOUS_WHEN_GTID_MODE_IS_ON.
SET GTID_NEXT = 'ANONYMOUS';
ERROR HY000: @@SESSION.GTID_NEXT cannot be set to ANONYMOUS when @@GLOBAL.GTID_MODE = ON.
include/assert.inc [GTID_NEXT == "ANONYMOUS"]
include/assert.inc [GTID_OWNED == ""]
# ANONYMOUS -> aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
include/assert.inc [GTID_NEXT == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
include/assert.inc [GTID_OWNED == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1"]
ROLLBACK;
SET GLOBAL GTID_MODE = 'ON_PERMISSIVE';
SET GTID_NEXT = 'ANONYMOUS';;
ROLLBACK;
SET GLOBAL GTID_MODE = 'ON';
# ANONYMOUS -> AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [GTID_NEXT == "AUTOMATIC"]
include/assert.inc [GTID_OWNED == ""]
ROLLBACK;
---- Clean up ----
SET GLOBAL GTID_MODE = ON_PERMISSIVE;
SET GLOBAL GTID_MODE = OFF_PERMISSIVE;
SET GLOBAL GTID_MODE = OFF;
SET GLOBAL ENFORCE_GTID_CONSISTENCY = OFF;
include/rpl_end.inc
