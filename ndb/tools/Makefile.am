
bin_PROGRAMS = drop_tab delete_all desc drop_index list_tables select_all select_count

delete_all_SOURCES = delete_all.cpp
desc_SOURCES = desc.cpp
drop_index_SOURCES = drop_index.cpp
drop_tab_SOURCES = drop_tab.cpp
list_tables_SOURCES = listTables.cpp
select_all_SOURCES = select_all.cpp
select_count_SOURCES = select_count.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapitest.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
