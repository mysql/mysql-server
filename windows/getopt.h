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

