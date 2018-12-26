# WL#2489: test of detection of functional dependencies
# for only_full_group_by

# Output differs in both modes, because
# - we print trace of FDs
# - but the detection is done only in PREPARE, not EXECUTE
# - in PS mode, the trace is of EXECUTE

# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc

--source include/group_by_fd.inc
