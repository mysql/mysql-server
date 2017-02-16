
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_MYSQL_INCL_HPP
#define DENA_MYSQL_INCL_HPP

#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H
#endif

#ifndef MYSQL_DYNAMIC_PLUGIN
#define MYSQL_DYNAMIC_PLUGIN
#endif

#define MYSQL_SERVER 1

#include <my_config.h>

#include <mysql_version.h>

#if MYSQL_VERSION_ID >= 50505
#include <my_pthread.h>
#include <sql_priv.h>
#include "sql_class.h"
#include "unireg.h"
#include "lock.h"
#include "key.h" // key_copy()
#include <my_global.h>
#include <mysql/plugin.h>
#include <transaction.h>
#include <sql_base.h>
// FIXME FIXME FIXME
#define safeFree(X) my_free(X)
#undef pthread_cond_timedwait
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#define pthread_cond_timedwait  mysql_cond_timedwait
#define  pthread_mutex_lock  mysql_mutex_lock
#define  pthread_mutex_unlock  mysql_mutex_unlock
#define current_stmt_binlog_row_based  is_current_stmt_binlog_format_row
#define clear_current_stmt_binlog_row_based  clear_current_stmt_binlog_format_row

#else
#include "mysql_priv.h"
#endif

#undef min
#undef max

#endif

