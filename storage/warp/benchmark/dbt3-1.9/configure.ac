AC_PREREQ(2.59)

dnl Process this file with autoconf to produce a configure script.
AC_INIT(dbt3, 1.9, osdldbt-general@lists.sourceforge.net)
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(strings.h sys/time.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Large file support?
AC_SYS_LARGEFILE

CFLAGS="-Wall $CFLAGS"
AC_CHECK_LIB(m, main)

dnl AC_ARG_ENABLE(tpch, [  --enable-tpch (default)],
dnl	ENABLE_TPCH="yes" ENABLE_TPCR="no", ENABLE_TPCH="yes" ENABLE_TPCR="no")
dnl AC_ARG_ENABLE(tpcr, [  --enable-tpcr], ENABLE_TPCR="yes" ENABLE_TPCH="no")
dnl if test "$ENABLE_TPCH" = "yes"; then
dnl   AC_DEFINE(TPCH, 1)
dnl fi
dnl if test "$ENABLE_TPCR" = "yes"; then
dnl   AC_DEFINE(TPCR, 1)
dnl fi

dnl> Configuration options for different databases, PostgreSQL is default.
AC_ARG_WITH(postgresql,
	[ AC_HELP_STRING([ --with-postgresql=DIR], [Default.])],
	[ PGSQL_PATH=$withval DATABASE_TO_USE="pgsql" SEPARATOR_FOR_DB="|" ],
	[ DATABASE_TO_USE="pgsql" SEPARATOR_FOR_DB="|" ])
AC_ARG_WITH(sapdb,
	[  --with-sapdb],
	[DATABASE_TO_USE="sapdb" SEPARATOR_FOR_DB=","], [])

TOPDIR=`pwd`
AC_SUBST(TOPDIR)
AC_SUBST(DATABASE_TO_USE)
AC_SUBST(SEPARATOR_FOR_DB)

AC_PATH_PROGS([SAR], [sar bsdsar], ,
             [$PATH:/usr/local/bin:/usr/bin])
if test -z "$SAR"; then
  AC_MSG_WARN([*** sar or bsdsar not found some scripts may not be usable])
  SAR=/usr/bin/sar
fi

AC_PATH_PROGS([IOSTAT], [iostat], ,
             [$PATH:/usr/local/bin:/usr/bin:/usr/X11R6/bin])
if test -z "$IOSTAT"; then
  AC_MSG_WARN([*** iostat not found some scripts may not be usable])
  IOSTAT=/usr/bin/iostat
fi

AC_PATH_PROGS([VMSTAT], [vmstat], ,
             [$PATH:/usr/local/bin:/usr/bin:/usr/X11R6/bin])
if test -z "$VMSTAT"; then
  AC_MSG_WARN([*** vmstat not found some scripts may not be usable])
  VMSTAT=/usr/bin/vmstat
fi

if test "$DATABASE_TO_USE" == "pgsql"; then
  AC_DEFINE(PGSQL, 1)

  dnl check for createdb
  AC_PATH_PROGS(CREATEDB, createdb, ,
    [$PATH:$PGSQL_PATH/bin:/bin:/usr/bin:/usr/local/bin]
  )
  if test -z "$CREATEDB" ; then
    AC_MSG_ERROR([*** createdb is required])
  fi

  dnl check for dropdb
  AC_PATH_PROGS(DROPDB, dropdb, ,
    [$PATH:$PGSQL_PATH/bin:/bin:/usr/bin:/usr/local/bin]
  )
  if test -z "$DROPDB" ; then
    AC_MSG_ERROR([*** dropdb is required])
  fi

  dnl check for psql
  AC_PATH_PROGS(PSQL, psql, ,
    [$PATH:$PGSQL_PATH/bin:/bin:/usr/bin:/usr/local/bin]
  )
  if test -z "$PSQL" ; then
    AC_MSG_ERROR([*** psql is required])
  fi

  dnl check for vacuumdb
  AC_PATH_PROGS(VACUUMDB, vacuumdb, ,
    [$PATH:$PGSQL_PATH/bin:/bin:/usr/bin:/usr/local/bin]
  )
  if test -z "$VACUUMDB" ; then
    AC_MSG_ERROR([*** vacuumdb is required])
  fi

  dnl check for vacuumdb
  AC_PATH_PROGS(PG_CTL, pg_ctl, ,
    [$PATH:$PGSQL_PATH/bin:/bin:/usr/bin:/usr/local/bin]
  )
  if test -z "$PG_CTL" ; then
    AC_MSG_ERROR([*** pg_ctl is required])
  fi

  dnl check for initdb
  AC_PATH_PROGS(INITDB, initdb, ,
    [$PATH:$PGSQL_PATH/bin:/bin:/usr/bin:/usr/local/bin]
  )
  if test -z "$INITDB" ; then
    AC_MSG_ERROR([*** initdb is required])
  fi

  AC_CONFIG_FILES([scripts/pgsql/get_query_plan.sh],
      [chmod +x scripts/pgsql/get_query_plan.sh])
  AC_CONFIG_FILES([scripts/pgsql/get_version.sh],
      [chmod +x scripts/pgsql/get_version.sh])
  AC_CONFIG_FILES([scripts/pgsql/run_power_query.sh],
      [chmod +x scripts/pgsql/run_power_query.sh])
  AC_CONFIG_FILES([scripts/pgsql/run_rf1.sh],
      [chmod +x scripts/pgsql/run_rf1.sh])
  AC_CONFIG_FILES([scripts/pgsql/run_rf2.sh],
      [chmod +x scripts/pgsql/run_rf2.sh])
  AC_CONFIG_FILES([scripts/pgsql/run_throughput_stream.sh],
      [chmod +x scripts/pgsql/run_throughput_stream.sh])
  AC_CONFIG_FILES([scripts/pgsql/create_db.sh],
      [chmod +x scripts/pgsql/create_db.sh])
  AC_CONFIG_FILES([scripts/pgsql/create_indexes.sh],
      [chmod +x scripts/pgsql/create_indexes.sh])
  AC_CONFIG_FILES([scripts/pgsql/create_tables.sh],
      [chmod +x scripts/pgsql/create_tables.sh])
  AC_CONFIG_FILES([scripts/pgsql/db_stats.sh],
      [chmod +x scripts/pgsql/db_stats.sh])
  AC_CONFIG_FILES([scripts/pgsql/drop_db.sh],
      [chmod +x scripts/pgsql/drop_db.sh])
  AC_CONFIG_FILES([scripts/pgsql/drop_tables.sh],
      [chmod +x scripts/pgsql/drop_tables.sh])
  AC_CONFIG_FILES([scripts/pgsql/grant.sh],
      [chmod +x scripts/pgsql/grant.sh])
  AC_CONFIG_FILES([scripts/pgsql/load_db.sh],
      [chmod +x scripts/pgsql/load_db.sh])
  AC_CONFIG_FILES([scripts/pgsql/pgsql_profile],
      [chmod +x scripts/pgsql/pgsql_profile])
  AC_CONFIG_FILES([scripts/pgsql/q_time.sh],
      [chmod +x scripts/pgsql/q_time.sh])
  AC_CONFIG_FILES([scripts/pgsql/start_db.sh],
      [chmod +x scripts/pgsql/start_db.sh])
  AC_CONFIG_FILES([scripts/pgsql/stop_db.sh],
      [chmod +x scripts/pgsql/stop_db.sh])
  AC_CONFIG_FILES([scripts/pgsql/update_statistics.sh],
      [chmod +x scripts/pgsql/update_statistics.sh])
  AC_CONFIG_FILES([scripts/pgsql/load_test.sh],
      [chmod +x scripts/pgsql/load_test.sh])
  AC_CONFIG_FILES([scripts/pgsql/power_test.sh],
      [chmod +x scripts/pgsql/power_test.sh])
  AC_CONFIG_FILES([scripts/pgsql/throughput_test.sh],
      [chmod +x scripts/pgsql/throughput_test.sh])
  AC_CONFIG_FILES([scripts/pgsql/throughput_stream_wrapper.sh],
      [chmod +x scripts/pgsql/throughput_stream_wrapper.sh])
  AC_CONFIG_FILES([scripts/pgsql/record_end.sh],
      [chmod +x scripts/pgsql/record_end.sh])
  AC_CONFIG_FILES([scripts/pgsql/record_start.sh],
      [chmod +x scripts/pgsql/record_start.sh])
elif test "$DATABASE_TO_USE" == "sapdb"; then
  AC_DEFINE(SAPDB, 1)

  AC_CONFIG_FILES([data_collect/sapdb/dbt3_stats.sh],
      [chmod +x data_collect/sapdb/dbt3_stats.sh])
  AC_CONFIG_FILES([data_collect/sapdb/power_test_stats.sh],
      [chmod +x data_collect/sapdb/power_test_stats.sh])
  AC_CONFIG_FILES([data_collect/sapdb/single_query_stats.sh],
      [chmod +x data_collect/sapdb/single_query_stats.sh])
  AC_CONFIG_FILES([data_collect/sapdb/throughput_query_stats.sh],
      [chmod +x data_collect/sapdb/throughput_query_stats.sh])
  AC_CONFIG_FILES([data_collect/sapdb/throughput_test_stats.sh],
      [chmod +x data_collect/sapdb/throughput_test_stats.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/explain_single_query.sh],
      [chmod +x dbdriver/scripts/sapdb/explain_single_query.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/explain_power_query.sh],
      [chmod +x dbdriver/scripts/sapdb/explain_power_query.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_dbt3.sh],
      [chmod +x dbdriver/scripts/sapdb/run_dbt3.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_load_test.sh],
      [chmod +x dbdriver/scripts/sapdb/run_load_test.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_single_query.sh],
      [chmod +x dbdriver/scripts/sapdb/run_single_query.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_perf_test.sh],
      [chmod +x dbdriver/scripts/sapdb/run_perf_test.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_power_query.sh],
      [chmod +x dbdriver/scripts/sapdb/run_power_query.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_power_test.sh],
      [chmod +x dbdriver/scripts/sapdb/run_power_test.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_refresh_stream.sh],
      [chmod +x dbdriver/scripts/sapdb/run_refresh_stream.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_rf1.sh],
      [chmod +x dbdriver/scripts/sapdb/run_rf1.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_rf2.sh],
      [chmod +x dbdriver/scripts/sapdb/run_rf2.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_throughput_query.sh],
      [chmod +x dbdriver/scripts/sapdb/run_throughput_query.sh])
  AC_CONFIG_FILES([dbdriver/scripts/sapdb/run_throughput_test.sh],
      [chmod +x dbdriver/scripts/sapdb/run_throughput_test.sh])
  AC_CONFIG_FILES([scripts/sapdb/backup_db.sh],
      [chmod +x scripts/sapdb/backup_db.sh])
  AC_CONFIG_FILES([scripts/sapdb/build_db.sh],
      [chmod +x scripts/sapdb/build_db.sh])
  AC_CONFIG_FILES([scripts/sapdb/create_db.sh],
      [chmod +x scripts/sapdb/create_db.sh])
  AC_CONFIG_FILES([scripts/sapdb/create_indexes.sh],
      [chmod +x scripts/sapdb/create_indexes.sh])
  AC_CONFIG_FILES([scripts/sapdb/create_tables.sh],
      [chmod +x scripts/sapdb/create_tables.sh])
  AC_CONFIG_FILES([scripts/sapdb/drop_tables.sh],
      [chmod +x scripts/sapdb/drop_tables.sh])
  AC_CONFIG_FILES([scripts/sapdb/load_db.sh],
      [chmod +x scripts/sapdb/load_db.sh])
  AC_CONFIG_FILES([scripts/sapdb/restore_db.sh],
      [chmod +x scripts/sapdb/restore_db.sh])
  AC_CONFIG_FILES([scripts/sapdb/sapdb_profile],
      [chmod +x scripts/sapdb/sapdb_profile])
  AC_CONFIG_FILES([scripts/sapdb/update_statistics.sh],
      [chmod +x scripts/sapdb/update_statistics.sh])
fi

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([scripts/dbt3_profile])
AC_CONFIG_FILES([scripts/get_composite.sh], [chmod +x scripts/get_composite.sh])
AC_CONFIG_FILES([scripts/get_config.sh], [chmod +x scripts/get_config.sh])
AC_CONFIG_FILES([scripts/gen_data.sh], [chmod +x scripts/gen_data.sh])
AC_CONFIG_FILES([scripts/gen_html.sh], [chmod +x scripts/gen_html.sh])
AC_CONFIG_FILES([scripts/run_workload.sh], [chmod +x scripts/run_workload.sh])
AC_CONFIG_FILES([scripts/start_sysstats.sh],
    [chmod +x scripts/start_sysstats.sh])
AC_CONFIG_FILES([scripts/stop_sysstats.sh], [chmod +x scripts/stop_sysstats.sh])
AC_CONFIG_FILES([src/Makefile])

AC_OUTPUT
