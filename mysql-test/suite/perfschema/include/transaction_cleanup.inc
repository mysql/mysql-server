# Tests for the performance schema

# ==========================================
# HELPER include/transaction_cleanup.inc
# ==========================================

DROP PROCEDURE clear_transaction_tables;
DROP PROCEDURE clear_transaction_history;
DROP PROCEDURE clear_statement_history;
DROP PROCEDURE clear_history;
DROP PROCEDURE transaction_verifier;

