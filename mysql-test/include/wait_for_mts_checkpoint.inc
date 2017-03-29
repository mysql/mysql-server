# ==== Purpose ====
#
# Waits until mts_checkpoint_routine has been fully
# executed - from the beginning till the end
#
SET @@global.debug= "+d,mts_checkpoint";
#If end was hit before start - clear it
SET DEBUG_SYNC="RESET";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_start";
SET DEBUG_SYNC="now WAIT_FOR mts_checkpoint_end";
SET @@global.debug= "-d,mts_checkpoint";
