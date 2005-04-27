include .defs.mk

TYPE =			*

NONPIC_ARCHIVE =	N

PIC_ARCHIVE =		Y

ARCHIVE_TARGET =	odbcexecutor

SOURCES = \
			Executor.cpp \
			Exec_query_lookup.cpp \
			Exec_query_index.cpp \
			Exec_query_scan.cpp \
			Exec_query_range.cpp \
			Exec_query_sys.cpp \
			Exec_pred_op.cpp \
			Exec_comp_op.cpp \
			Exec_expr_op.cpp \
			Exec_expr_func.cpp \
			Exec_expr_conv.cpp \
			Exec_insert.cpp \
			Exec_update_lookup.cpp \
			Exec_update_index.cpp \
			Exec_update_scan.cpp \
			Exec_delete_lookup.cpp \
			Exec_delete_index.cpp \
			Exec_delete_scan.cpp \
			Exec_create_table.cpp \
			Exec_create_index.cpp \
			Exec_drop_table.cpp \
			Exec_drop_index.cpp

include ../Extra.mk
include $(NDB_TOP)/Epilogue.mk
