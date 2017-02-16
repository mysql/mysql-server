/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       main.h
/// \brief      Miscellanous declarations
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

/// Possible exit status values. These are the same as used by gzip and bzip2.
enum exit_status_type {
	E_SUCCESS  = 0,
	E_ERROR    = 1,
	E_WARNING  = 2,
};


/// Sets the exit status after a warning or error has occurred. If new_status
/// is E_WARNING and the old exit status was already E_ERROR, the exit
/// status is not changed.
extern void set_exit_status(enum exit_status_type new_status);


/// Use E_SUCCESS instead of E_WARNING if something worth a warning occurs
/// but nothing worth an error has occurred. This is called when --no-warn
/// is specified.
extern void set_exit_no_warn(void);


/// Exits the program using the given status. This takes care of closing
/// stdin, stdout, and stderr and catches possible errors. If we had got
/// a signal, this function will raise it so that to the parent process it
/// appears that we were killed by the signal sent by the user.
extern void my_exit(enum exit_status_type status) lzma_attribute((noreturn));
