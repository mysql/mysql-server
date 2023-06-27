# This script reverts what has been modified by
# stop_dirty_page_flushing_and_redo_log_truncating.inc

--source resume_dirty_page_flushing.inc

--source resume_redo_log_truncation.inc
