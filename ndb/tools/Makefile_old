include .defs.mk

BIN_DIRS = select_all select_count desc list_tables \
	drop_tab delete_all copy_tab \
	create_index drop_index verify_index cpcc

ifneq ($(NDB_ODBC),N)
BIN_DIRS += ndbsql
endif

include $(NDB_TOP)/Epilogue.mk

