--disable_query_log
CALL mtr.add_suppression("Skip re-populating collations and character sets tables in InnoDB read-only mode.");
CALL mtr.add_suppression("Skip updating information_schema metadata in InnoDB read-only mode.");
CALL mtr.add_suppression("Skipped updating resource group metadata in InnoDB read only mode.");
--enable_query_log
