include .defs.mk

TYPE =			*

NONPIC_ARCHIVE =	N

PIC_ARCHIVE =		Y

ARCHIVE_TARGET =	odbccodegen

SOURCES = \
			SimpleScan.lpp \
			SimpleGram.ypp \
			SimpleParser.cpp \
			CodeGen.cpp \
			Code_base.cpp \
			Code_root.cpp \
			Code_stmt.cpp \
			Code_query.cpp \
			Code_dml.cpp \
			Code_ddl.cpp \
			Code_select.cpp \
			Code_pred.cpp \
			Code_pred_op.cpp \
			Code_comp_op.cpp \
			Code_query_project.cpp \
			Code_query_filter.cpp \
			Code_query_join.cpp \
			Code_query_lookup.cpp \
			Code_query_index.cpp \
			Code_query_scan.cpp \
			Code_query_range.cpp \
			Code_query_sys.cpp \
			Code_query_repeat.cpp \
			Code_query_count.cpp \
			Code_query_sort.cpp \
			Code_query_group.cpp \
			Code_query_distinct.cpp \
			Code_expr_row.cpp \
			Code_expr.cpp \
			Code_expr_op.cpp \
			Code_expr_func.cpp \
			Code_expr_conv.cpp \
			Code_expr_column.cpp \
			Code_expr_const.cpp \
			Code_expr_param.cpp \
			Code_update.cpp \
			Code_update_lookup.cpp \
			Code_update_index.cpp \
			Code_update_scan.cpp \
			Code_set_row.cpp \
			Code_insert.cpp \
			Code_dml_row.cpp \
			Code_dml_column.cpp \
			Code_delete.cpp \
			Code_delete_lookup.cpp \
			Code_delete_index.cpp \
			Code_delete_scan.cpp \
			Code_column.cpp \
			Code_table_list.cpp \
			Code_table.cpp \
			Code_create_table.cpp \
			Code_create_index.cpp \
			Code_create_row.cpp \
			Code_ddl_row.cpp \
			Code_ddl_column.cpp \
			Code_ddl_constr.cpp \
			Code_idx_column.cpp \
			Code_data_type.cpp \
			Code_drop_table.cpp \
			Code_drop_index.cpp

ifeq ($(NDB_OS),WIN32)
CCFLAGS += -I$(call fixpath,.)

_libs:: FlexLexer.h unistd.h

endif

include ../Extra.mk
include $(NDB_TOP)/Epilogue.mk

ifeq ($(NDB_OS),LINUX)
FLEXHACK = perl -i -pe 's/\bisatty\b/ouencunbwdb2y1bdc/g'
BISONHACK = perl -i -pe 's/^\s*__attribute__\s*\(\(.*\)\)//'
endif

ifeq ($(NDB_OS),MACOSX)
FLEXHACK = perl -i -pe 's/\bisatty\b/ouencunbwdb2y1bdc/g'
BISONHACK = perl -i -pe 's/^\s*__attribute__\s*\(\(.*\)\)//'
endif

ifeq ($(NDB_OS),SOLARIS)
BISONHACK = perl -i -pe 's/^\s*__attribute__\s*\(\(.*\)\)//'
endif

ifeq ($(NDB_OS),WIN32)
unistd.h:
	touch unistd.h

FlexLexer.h:
	cp /usr/include/FlexLexer.h .

endif
