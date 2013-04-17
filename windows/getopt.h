/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef _TOKUWIN_GETOPT_H
#define _TOKUWIN_GETOPT_H

#if defined(__cplusplus)
extern "C" {
#endif

int
getopt(int argc, char *const argv[], const char *optstring);

extern char *optarg;
extern int  optind;

#if defined(__cplusplus)
};
#endif

#endif

