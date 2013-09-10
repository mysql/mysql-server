/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       util.h
/// \brief      Miscellaneous utility functions
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

/// \brief      Safe malloc() that never returns NULL
///
/// \note       xmalloc(), xrealloc(), and xstrdup() must not be used when
///             there are files open for writing, that should be cleaned up
///             before exiting.
#define xmalloc(size) xrealloc(NULL, size)


/// \brief      Safe realloc() that never returns NULL
extern void *xrealloc(void *ptr, size_t size);


/// \brief      Safe strdup() that never returns NULL
extern char *xstrdup(const char *src);


/// \brief      Fancy version of strtoull()
///
/// \param      name    Name of the option to show in case of an error
/// \param      value   String containing the number to be parsed; may
///                     contain suffixes "k", "M", "G", "Ki", "Mi", or "Gi"
/// \param      min     Minimum valid value
/// \param      max     Maximum valid value
///
/// \return     Parsed value that is in the range [min, max]. Does not return
///             if an error occurs.
///
extern uint64_t str_to_uint64(const char *name, const char *value,
		uint64_t min, uint64_t max);


/// \brief      Convert uint64_t to a string
///
/// Convert the given value to a string with locale-specific thousand
/// separators, if supported by the snprintf() implementation. The string
/// is stored into an internal static buffer indicated by the slot argument.
/// A pointer to the selected buffer is returned.
///
/// This function exists, because non-POSIX systems don't support thousand
/// separator in format strings. Solving the problem in a simple way doesn't
/// work, because it breaks gettext (specifically, the xgettext tool).
extern const char *uint64_to_str(uint64_t value, uint32_t slot);


/// \brief      Convert double to a string with one decimal place
///
/// This is like uint64_to_str() except that this converts a double and
/// uses exactly one decimal place.
extern const char *double_to_str(double value);


/// \brief      Check if filename is empty and print an error message
extern bool is_empty_filename(const char *filename);


/// \brief      Test if stdin is a terminal
///
/// If stdin is a terminal, an error message is printed and exit status set
/// to EXIT_ERROR.
extern bool is_tty_stdin(void);


/// \brief      Test if stdout is a terminal
///
/// If stdout is a terminal, an error message is printed and exit status set
/// to EXIT_ERROR.
extern bool is_tty_stdout(void);
