/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
 * The utilities in stacktrace wrap the corresponding backtrace utilities,
 * ensuring that the internal backtrace state is created at most once.
 */
#pragma once

#include <cstdint>

namespace stacktrace {
inline namespace callback {
/**
 * The type of the error callback argument to backtrace functions. This
 * function, if not NULL, will be called for certain error cases.
 *
 * @param[in,out] ctx to communicate state.
 * @param[in] msg an error message. which may become invalid after the call. if
 *                no debug info, it will be something along the lines of "no
 *                debug info"
 * @param[in] errnum  if greater than 0, holds an errno value. If -1, no debug
 *                    info can be found for the executable, or if the debug info
 *                    exists but has an unsupported version, but the function
 *                    requires debug info (e.g., backtrace_full,
 *                    backtrace_pcinfo); alternatively, no symbol table exists,
 *                    but the function requires a symbol table (e.g.,
 *                    backtrace_syminfo). This may be used as a signal that some
 *                    other approach should be tried.
 */
using error_t = void (*)(void *ctx, const char *msg, int errnum);

/**
 * The type of the callback argument to the backtrace_full function.
 *
 * @param[in,out] ctx to communicate state.
 * @param[in] pc the program counter.
 * @param[in] filename the name of the file containing pc, or nullptr. May
 *                     become invalid after the call.
 * @param[in] lineno the line number in filename containing pc, or 0. May
 *                   become invalid after the call.
 * @param[in] function the name of the function containing pc, nullptr.
 * @returns 0 to continuing tracing.
 */
using full_t = int (*)(void *ctx, uintptr_t pc, const char *filename,
                       int lineno, const char *function);

/**
 * The type of the callback argument to the backtrace_simple function.
 *
 * @param[in,out] ctx to communicate state.
 * @param[in] pc the program counter.
 * @returns 0 to continue tracing.
 */
using simple_t = int (*)(void *ctx, uintptr_t pc);

/**
 * @param[in,out] ctx to communicate state.
 * @param[in] pc the program counter.
 * @param[in] symname the name of the symbol for the corresponding code. nullptr
 *                    if no error occurred but the symbol could not be found.
 * @param[in] symval the value of the symbol.
 * @param[in] symsize the size of the symbol.
 */
using syminfo_t = void (*)(void *ctx, uintptr_t pc, const char *symname,
                           uintptr_t symval, uintptr_t symsize);
}  // namespace callback

/**
 * Get a full stack backtrace given debug info for the executable.
 *
 * @param[in] skip the number of frames to skip; passing 0 will start the trace
 *                 with the caller.
 * @param[in] callback function called per frame.
 * @param[in] error_callback function in case of error.
 * @param[in] ctx passed to callback/error_callback.
 *
 * If any call to callback returns a non-zero value, the stack backtrace stops;
 * this may be used to limit the number of stack frames desired.
 */
void full(int skip, full_t callback, error_t error_callback, void *ctx);

/**
 * Get a simple backtrace. No debug info needed for the executable.
 *
 * @param[in] skip the number of frames to skip; passing 0 will start the trace
 *                 with the caller.
 * @param[in] callback function called per frame.
 * @param[in] error_callback function called in case of error.
 * @param[in] ctx passed to callback/error_callback.
 *
 * If any call to callback returns a non-zero value, the stack backtrace stops;
 * this may be used to limit the number of stack frames desired.
 */
void simple(int skip, simple_t callback, error_t error_callback, void *ctx);

/**
 * Given pc, a program counter in the current program, call the callback
 * function with filename, line number, and function name information. This will
 * normally call the callback function exactly once. However, if the pc happens
 * to describe an inlined call, and the debugging information contains the
 * necessary information, then this may call the callback function multiple
 * times.
 *
 * @param[in] pc the program counter
 * @param[in] callback function called once per frame normally. It may be called
 *                     multiple times if the pc describes an inlined call and
 *                     the necessary debugging information exists.
 * @param[in] error_callback function called in case of error.
 * @param[in] ctx passed to callback/error_callback.
 */
void pcinfo(uintptr_t pc, full_t callback, error_t error_callback, void *ctx);

/**
 * Given addr, call the callback information with the symbol name and value
 * describing the function or variable in which addr may be found. This function
 * requires the symbol table but does not require the debug info.
 *
 * @param[in] addr an address or program counter in the current program.
 * @param[in] callback function called once.
 * @param[in] error_callback function called in case of error.
 * @param[in] ctx passed to callback/error_callback.
 *
 * @note if the symbol table is present but addr could not be found in the
 * table, callback will be called with a nullptr synname.
 */
void syminfo(uintptr_t addr, syminfo_t callback, error_t error_callback,
             void *ctx);
}  // namespace stacktrace
