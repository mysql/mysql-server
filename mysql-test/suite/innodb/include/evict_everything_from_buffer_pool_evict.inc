#
# Scan the prepared eviction table to push other pages out of the buffer pool.
#

-- disable_result_log
SELECT * FROM _big WHERE a = 'find nothing';
-- enable_result_log
