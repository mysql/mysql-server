# WL#2489: test of detection of functional dependencies
# for only_full_group_by

# Output differs in both modes, because
# - we print trace of FDs
# - but the detection is done only in PREPARE, not EXECUTE
# - in PS mode, the trace is of EXECUTE

if (`SELECT $PS_PROTOCOL + $SP_PROTOCOL + $CURSOR_PROTOCOL
            + $VIEW_PROTOCOL > 0`)
{
   --skip Need normal protocol
}

--source include/group_by_fd.inc
