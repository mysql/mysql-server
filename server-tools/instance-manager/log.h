#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_LOG_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_LOG_H
/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Logging facilities. 

  Two logging streams are supported: error log and info log. Additionally
  libdbug may be used for debug information output.
  ANSI C buffered I/O is used to perform logging.
  Logging is performed via stdout/stder, so one can reopen them to point to
  ordinary files. To initialize loggin environment log_init() must be called.
  
  Rationale:
  - no MYSQL_LOG as it has BIN mode, and not easy to fetch from sql_class.h
  - no constructors/desctructors to make logging available all the time
  Function names are subject to change.
*/


/* Precede error message with date and time and print it to the stdout */
void log_info(const char *format, ...)
#ifdef __GNUC__
        __attribute__ ((format(printf, 1, 2)))
#endif
  ;


/* Precede error message with date and time and print it to the stderr */
void log_error(const char *format, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 1, 2)))
#endif
  ;


/*
  Now this is simple catchouts for printf (no date/time is logged), to be
  able to replace underlying streams in future.
*/

void print_info(const char *format, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 1, 2)))
#endif
  ;


void print_error(const char *format, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 1, 2)))
#endif
  ;

/*  initialize logs */
void log_init();


/* print information to the error log and eixt(1) */  

void die(const char *format, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 1, 2)))
#endif
  ;

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_LOG_H
