
ndbtools_PROGRAMS = \
  ndb_test_platform \
  ndb_waiter \
  ndb_drop_table \
  ndb_delete_all \
  ndb_desc \
  ndb_drop_index \
  ndb_show_tables \
  ndb_select_all \
  ndb_select_count \
  ndb_restore

tools_common_sources = ../test/src/NDBT_ReturnCodes.cpp \
                       ../test/src/NDBT_Table.cpp \
                       ../test/src/NDBT_Output.cpp

ndb_test_platform_SOURCES = ndb_test_platform.cpp
ndb_waiter_SOURCES = waiter.cpp $(tools_common_sources)
ndb_delete_all_SOURCES = delete_all.cpp $(tools_common_sources)
ndb_desc_SOURCES = desc.cpp $(tools_common_sources)
ndb_drop_index_SOURCES = drop_index.cpp $(tools_common_sources)
ndb_drop_table_SOURCES = drop_tab.cpp $(tools_common_sources)
ndb_show_tables_SOURCES = listTables.cpp $(tools_common_sources)
ndb_select_all_SOURCES = select_all.cpp \
                         ../test/src/NDBT_ResultRow.cpp \
                         $(tools_common_sources)
ndb_select_count_SOURCES = select_count.cpp $(tools_common_sources)
ndb_restore_SOURCES = restore/main.cpp \
                      restore/consumer.cpp \
                      restore/consumer_restore.cpp \
                      restore/consumer_printer.cpp \
                      restore/Restore.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapitools.mk.am

ndb_test_platform_LDFLAGS = @ndb_bin_am_ldflags@
ndb_waiter_LDFLAGS = @ndb_bin_am_ldflags@
ndb_drop_table_LDFLAGS = @ndb_bin_am_ldflags@
ndb_delete_all_LDFLAGS = @ndb_bin_am_ldflags@
ndb_desc_LDFLAGS = @ndb_bin_am_ldflags@
ndb_drop_index_LDFLAGS = @ndb_bin_am_ldflags@
ndb_show_tables_LDFLAGS = @ndb_bin_am_ldflags@
ndb_select_all_LDFLAGS = @ndb_bin_am_ldflags@
ndb_select_count_LDFLAGS = @ndb_bin_am_ldflags@
ndb_restore_LDFLAGS = @ndb_bin_am_ldflags@

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: \
  ndb_waiter.dsp \
  ndb_drop_table.dsp \
  ndb_delete_all.dsp \
  ndb_desc.dsp \
  ndb_drop_index.dsp \
  ndb_show_tables.dsp \
  ndb_select_all.dsp \
  ndb_select_count.dsp

ndb_waiter.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ ndb_waiter
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_waiter_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

ndb_drop_table.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ ndb_drop_table
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_drop_table_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

ndb_delete_all.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ ndb_delete_all
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_delete_all_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

ndb_desc.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ ndb_desc
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_desc_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

ndb_drop_index.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ ndb_drop_index
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_drop_index_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

ndb_show_tables.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ ndb_show_tables
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_show_tables_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

ndb_select_all.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ ndb_select_all
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_select_all_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)

ndb_select_count.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ ndb_select_count
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndb_select_count_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)
