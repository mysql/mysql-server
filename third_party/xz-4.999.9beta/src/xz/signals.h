/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       signals.h
/// \brief      Handling signals to abort operation
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

/// If this is true, we will clean up the possibly incomplete output file,
/// return to main() as soon as practical. That is, the code needs to poll
/// this variable in various places.
extern volatile sig_atomic_t user_abort;


/// Initialize the signal handler, which will set user_abort to true when
/// user e.g. presses C-c.
extern void signals_init(void);


#ifndef _WIN32

/// Block the signals which don't have SA_RESTART and which would just set
/// user_abort to true. This is handy when we don't want to handle EINTR
/// and don't want SA_RESTART either.
extern void signals_block(void);

/// Unblock the signals blocked by signals_block().
extern void signals_unblock(void);

/// If user has sent us a signal earlier to terminate the process,
/// re-raise that signal to actually terminate the process.
extern void signals_exit(void);

#else

#define signals_block() do { } while (0)
#define signals_unblock() do { } while (0)
#define signals_exit() do { } while (0)

#endif
