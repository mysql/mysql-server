/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * @class mysql_harness::Loader
 *
 * @ingroup Loader
 *
 *
 *
 * ## Introduction
 *
 * The loader class is responsible for managing the life-cycle of
 * plugins in the harness. Each plugin goes through seven steps in the
 * life-cycle, of which steps #2, #3, #5 and #6 are optional:
 *
 * 1. Loading
 * 2. Initialization
 * 3. Starting
 * 4. Running
 * 5. Stopping
 * 6. Deinitialization
 * 7. Unloading
 *
 * ## Overview of Life-cycle Steps
 *
 * ### 1. Loading ###
 *
 * When *loading*, the plugin is loaded using the dynamic library
 * support available on the operating system. Symbols are evaluated
 * lazily (for example, the `RTLD_LAZY` flag is used for `dlopen`) to
 * allow plugins to be loaded in any order. The symbols that are
 * exported by the plugin are made available to all other plugins
 * loaded (flag `RTLD_GLOBAL` to `dlopen`).
 *
 * As part of the loading procedure, the *plugin structure* (see
 * Plugin class) is fetched from the module and used for the four
 * optional steps below.
 *
 *
 *
 * ### 2. Initialization ###
 *
 * After all the plugins are successfully loaded, each plugin is given
 * a chance to perform initialization. This step is only executed if
 * the plugin structure defines an `init` function. Note that it is
 * guaranteed that the init function of a plugin is called *after* the
 * `init` function of all plugins it requires have been called. The
 * list of these dependencies is specified via `requires` field of the
 * `Plugin` struct.
 *
 * @note if some plugin `init()` function fails, any plugin `init()`
 * functions schedulled to run after will not run, and harness will
 * proceed straight to deinitialization step, bypassing calling
 * `start()` and `stop()` functions.
 *
 *
 *
 * ### 3. Starting ###
 * After all plugins have been successfully initialized, a thread is
 * created for each plugin that has a non-NULL `start` field in the
 * plugin structure. The threads are started in an arbitrary order,
 * so you have to be careful about not assuming that, for example,
 * other plugins required by the plugin have started their thread. If
 * the plugin does not define a `start` function, no thread is created.
 * There is a "running" flag associated with each such thread; this
 * flag is set when the thread starts but before the `start` function
 * is called. If necessary, the plugin can spawn more threads using
 * standard C++11 thread calls, however, these threads should not
 * call harness API functions.
 *
 *
 *
 * ### 4. Running ###
 * After starting all plugins (that needed to be started), the harness
 * will enter the *running step*. This is the "normal" phase, where the
 * application spends most of its lifetime (application and plugins
 * service requests or do whatever it is they do). Harness will remain
 * in this step until one of two things happen:
 *
 *   1. shutdown signal is received by the harness
 *   2. one of the plugins exits with error
 *
 * When one of these two events occurs, harness progresses to the
 * next step.
 *
 *
 *
 * ### 5. Stopping ###
 * In this step, harness "tells" plugins running `start()` to exit this
 * function by clearing the "running" flag. It also invokes `stop()`
 * function for all plugins that provided it. It then waits for all
 * running plugin threads to exit.
 *
 * @note under certain circumstances, `stop()` may overlap execution
 *       with `start()`, or even be called before `start()`.
 *
 *
 *
 * ### 6. Deinitialization ###
 * After all threads have stopped, regardless of whether they stopped
 * with an error or not, the plugins are deinitialized in reverse order
 * of initialization by calling the function in the `deinit` field of
 * the `Plugin` structure. Regardless of whether the `deinit()` functions
 * return an error or not, all plugins schedulled for deinitialisation
 * will be deinitialized.
 *
 * @note for any `init()` functions that failed, `deinit()` functions
 *       will not run.
 * @note plugins may have a `deinit()` function despite not having a
 *       corresponding `init()`. In such cases, the missing `init()` is
 *       treated as if it existed and ran successfully.
 *
 *
 *
 * ### 7. Unloading ###
 * After a plugin has deinitialized, it can be unloaded. It is
 * guaranteed that no module is unloaded before it has been
 * deinitialized.
 *
 * @note This step is currently unimplemented - meaning, it does nothing.
 *       The plugins will remain loaded in memory until the process shuts
 *       down. This makes no practical difference on application behavior
 *       at present, but might be needed if Harness gained ability to
 *       reload plugins in the future.

## Behavior Diagrams

Previous section described quickly each step of the life-cycle process. In this
section, two flow charts are presented which show the operation of all seven
steps. First shows a high-level overview, and the second shows all 7 life-cycle
steps in more detail. Discussion of details follows in the following sections.

Some points to keep in mind while viewing the diagrams:

- diagrams describe code behavior rather than implementation. So for example:
  - pseudocode does not directly correspond 1:1 to real code. However, it
    behaves exactly like the real code.

- seven life-cycle functions shown are actual functions (@c Loader's methods,
  to be more precise)
  - load_all(), init_all(), start_all(), main_loop(), stop_all(), deinit_all()
    are implemented functions (first 6 steps of life-cycle)
  - unload_all() is the 7th step of life-cycle, but it's currently unimplemented

- when plugin functions exit with error, they do so by calling
  set_error() before exiting

- some things are not shown to keep the diagram simple:
  - first error returned by any of the 7 life-cycle functions is
    saved and passed at the end of life-cycle flow to the calling code


### Overview

@verbatim

\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
\                                                                              \
\   START                                                                      \
\     |                                                                        \
\     |                                                                        \
\     |                                                                        \
\     V                                                                        \
\   [load_all()]                                                               \
\     |                                                                        \
\     V                                                                        \
\   <LOAD_OK?>                                                                 \
\     |   |                                                                    \
\ +---N   Y                                                                    \
\ |       |                                                                    \
\ |       v                                                                    \
\ | [init_all()]                                                               \
\ |   |                                                                        \
\ |   v                                                                        \
\ | <INIT_OK?>                                         (  each plugin runs  )  \
\ |   |   |                                            (in a separate thread)  \
\ |   N   Y                                                                    \
\ |   |   |                                             [plugin[1]->start()]   \
\ |   |   v             start plugin threads            [plugin[2]->start()]   \
\ |   | [start_all()] - - - - - - - - - - - - - - - - ->[    ..      ..    ]   \
\ |   |   |                                             [    ..      ..    ]   \
\ |   |   |  + - - - - - - - - - - - - - - - - - - - - -[plugin[n]->start()]   \
\ |   |   |            notification when each                     ^            \
\ |   |   |  |           thread terminates                                     \
\ |   |   |                                                       |            \
\ |   |   |  |                                                     stop plugin \
\ |   |   |                                                       |  threads   \
\ |   |   |  |                                                                 \
\ |   |   |                                                       |            \
\ |   |   v  v                                                                 \
\ |   | [main_loop()]= call ==>[stop_all()] - - - - - - - - - - - +            \
\ |   |   |                                                                    \
\ |   |   |        \                                                           \
\ |   *<--+         \                                                          \
\ |   |              \__ waits for all plugin                                  \
\ |   v                  threads to terminate                                  \
\ | [deinit_all()]                                                             \
\ |   |                                                                        \
\ |   v                                                                        \
\ +-->*                                                                        \
\     |                                                                        \
\     v                                                                        \
\   [unload_all()]                                                             \
\     |                                                                        \
\     |         \                                                              \
\     |          \                                                             \
\     v           \__ currently not implemented                                \
\    END                                                                       \
\                                                                              \
\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

@endverbatim


### Detailed View

@verbatim

            START
              |
              |
              v
\\\\ load_all() \\\\\\\\\\\\\\\\\\\\\\
\                                    \
\  LOAD_OK = true                    \
\  foreach plugin:                   \
\    load plugin                     \
\    if (exit_status != ok):         \
\      LOAD_OK = false               \
\      break loop                    \
\                                    \
\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
              |
              |
              v
            <LOAD_OK?>
              |   |
              Y   N----> unload_all() (see further down)
              |
              |
\\\\\\\\\\\\\\|\\\ init_all() \\\\\\\\\\\\\\\\\
\             |                               \
\             v                               \
\           [INIT_OK = true, i = 1]           \
\             |                               \
\             v                               \
\     +----><plugin[i] exists?>               \
\     |       |   |                           \
\   [i++]     Y   N---------------------+     \
\     ^       |                         |     \
\     |       |                         |     \
\     |       v                         |     \
\     |     <plugin[i] has init()?>     |     \
\     |       |   |                     |     \
\     |       N   Y---+                 |     \
\     |       |       |                 |     \
\     |       |       |                 |     \
\     |       |       v                 |     \
\     |       |     [plugin[i]->init()] |     \
\     |       |       |                 |     \
\     |       |       |                 |     \
\     |       |       |                 |     \
\     |       |       |                 |     \
\     |       |       v                 |     \
\     |       |     <exit ok?>          |     \
\     |       v       |   |             |     \
\     +-------*<------Y   N             |     \
\                         |             |     \
\                         |             |     \
\                         v             |     \
\                   [INIT_OK = false]   |     \
\                     |                 |     \
\                     v                 |     \
\                     *<----------------+     \
\                     |                       \
\                     v                       \
\                   [LAST_PLUGIN = i-1]       \
\                     |                       \
\\\\\\\\\\\\\\\\\\\\\\|\\\\\\\\\\\\\\\\\\\\\\\\
                      |
                      |
                      v
                    <INIT_OK?>
                      |   |
                      Y   N----> deinit_all() (see further down)
                      |
                      |
                      v
\\\\ start_all() \\\\\\\\\\\\\\\\\\\\\\\\\
\                                        \
\   for i = 1 to LAST_PLUGIN:            \
\     if plugin[i] has start():          \  start start() in new thread
\       new thread(plugin[i]->start()) - - - - - - - - - - - - - - - - +
\                                        \
\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\                             |
                      |
                      |                                                |
    +-----------------+
    |                                                                  |
\\\\|\\\ main_loop() \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ \\\\\\\\
    |                                                                  |       \
    v                                                                          \
+-->*                                                                  |       \
|   |                                                                          \
|   v                                                                  |       \
| <any plugin threads running?>                                                \
|   |   |                                                              |       \
|   N   Y---+                                                                  \
|   |       |                                                          |       \
|   |     <shutdown signal received && stop_all() not called yet?>             \
|   |       |   |                                                      |       \
|   |       N   Y                                                              \
|   |       |    == call ==>[stop_all()]- - - - - - - - - - - - - +    |       \
|   |       |   |                     tell (each) start() to exit              \
|   |       *<--+                                                 |    |       \
|   |       |                                                                  \
|   |       |                                                     v    v       \
|   |       |                                             [plugin[1]->start()] \
|   |       v                 (one) plugin thread exits   [plugin[2]->start()] \
|   |     [wait for (any)]<- - - - - - - - - - - - - - - -[    ..      ..    ] \
|   |     [ thread exit  ]                                [    ..      ..    ] \
|   |       |                                             [plugin[n]->start()] \
|   |       |                                                     ^            \
|   |       |                                                                  \
|   |       v                                                     |            \
|   |     <thread exit ok?>                                                    \
|   |       |   |                                                 |            \
|   |       Y   N---+                                                          \
|   |       |       |                                             |            \
|   |       |       v                                                          \
|   |       |     <stop_all() called already?>                    |            \
|   |       v       |   |                                                      \
|   |       *<------Y   N                                    tell (each)       \
|   |       |            = call ==+                        start() to exit     \
|   |       |           |         |                               |            \
|   |       v           |         |                                            \
+---|-------*<----------+         *==>[stop_all()]- - - - - - - - +            \
    |                             |                                            \
    |                             |        |                                   \
    v                             |        |                                   \
  <stop_all() called already?>    |        |                                   \
    |   |                         |        |                                   \
    Y   N                         |        |                                   \
    |    == call =================+        |                                   \
    |   |                                  |                                   \
    *---+                                  |                                   \
    |                                      |                                   \
\\\\|\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\|\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
    |                                      |
    |                                      |
    v                                      |
    *<---- init_all() (if !INIT_OK)        |
    |                                      |
    |                                      |
    v                                      |
\\\\ deinit_all() \\\\\\\\\\\\\\\\\\\\     |
\                                    \     |
\  for i = LAST_PLUGIN to 1:         \     |
\    if plugin[i] has deinit():      \     |
\      plugin[i]->deinit()           \     |
\      if (exit_status != ok):       \     |
\        # ignore error              \     |
\                                    \     |
\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\     |
    |                                      |
    |                                      |
    v                                      |
    *<---- load_all() (if !LOAD_OK)        |
    |                                      |
    v                                      |
\\\\ unload_all() \\\\\\\\\\\\\\\\\\\\     |
\                                    \     |
\  no-op (currently unimplemented)   \     |
\                                    \     |
\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\     |
    |                                      |
    |                                      |
    v                                      /
   END                                    /
                                         /
                                        /
                                       /                  (  each plugin runs  )
                                      /                   (in a separate thread)
\\\\ stop_all() \\\\\\\\\\\\\\\\\\\\\\  run_flag == false
\                                    \    tells start()    [plugin[1]->start()]
\  for i = 1 to LAST_PLUGIN:         \      to exit        [plugin[2]->start()]
\    run_flag[i] = false - - - - - - - - - - - - - - - - ->[    ..      ..    ]
\    if plugin[i] has stop():        \                     [    ..      ..    ]
\      plugin[i]->stop()             \                     [    ..      ..    ]
\      if (exit_status != ok):       \                     [plugin[n]->start()]
\        # ignore error              \
\                                    \
\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

@endverbatim




## Discussion

### Persistence (definition)

Before continuing, we need to define the word "persist", used later on. When we
say "persist", we'll mean the opposite of "exit". So when we say a function or
a thread persists, it means that it's not returning/exiting, but instead is
running some forever-loop or is blocked on something, etc. What it's doing
exactly doesn't matter, what matters, is that it hasn't terminated but
"lives on" (in case of a function, to "persist" means the same as to "block",
but for threads that might sound confusing, this is why we need a new word).
So when we call start() in a new thread, it will either run and keep running
(thread will persist), or it will run briefly and return (thread will exit).
In short, "to persist" means the opposite of "to finish running and exit".

### Plugin API functions

Each plugin can define none, any or all of the following 4 callbacks.
They're function pointers, that can be null if not implemented. They're
typically called in the order as listed below (under certain circumstances,
stop() may overlap execution with start(), or even be called before start()).

- init()    -- called inside of main thread
- start()   -- main thread creates a new thread, then calls this
- stop()    -- called inside of main thread
- deinit()  -- called inside of main thread

### Starting and Stopping: Start() ###

It is typical to implement start function in such a way that it will
"persist" (i.e. it will run some forever-loop processing requests
rather than exit briefly after being called). In such case, Harness
must have a way to terminate it during shutdown operation.

For this purpose, Harness exposes a boolean "running" flag to each plugin, which
serves as means to communicate the need to shutdown; it is read by
`is_running()` function. This function should be routinely polled by plugin's
`start()` function to determine if it should shut down, and once it returns
false, plugin `start()` should terminate as soon as possible. Failure to
terminate will block Harness from progressing further in its shutdown procedure,
resulting in application "hanging" during shutdown. Typically, `start()`
would be implemented more-or-less like so:

    void start()
    {
      // run-once code

      while (is_running())
      {
        // forever-loop code
      }

      // clean-up code
    }

There is also an alternative blocking function available, `wait_for_stop()`,
should that be better suited for the particular plugin design. Instead of
quickly returning a boolean flag, it will block (with an optional timeout)
until Harness flags to shut down this plugin. It is an efficient functional
equivalent of:

    while (is_running())
    {
      // sleep a little or break on timeout
    }

When entering shutdown phase, Harness will notify all plugins to shut down
via mechanisms described above. It is also permitted for plugins to exit on
their own, whether due to error or intended behavior, without consulting
this "running" flag. Polling the "running" flag is only needed when `start()`
"persists" and does not normally exit until told to do so.

Also, in some designs, `start()` function might find it convenient to be able to
set the "running" flag to false, in order to trigger its own shutdown in another
piece of code. For such cases, `clear_running()` function is provided, which
will do exactly that.

IMPORTANT! Please note that all 3 functions described above (`is_running()`,
`wait_for_stop()` and `clear_running()`) can only be called from a thread
running `start()` function. If `start()` spawns more theads, these
functions CANNOT be called from them. These functions also cannot be called
from the other three plugin functions (`init()`, `stop()` and `deinit()`).

### Starting and Stopping: Stop() ###

During shutdown, or after plugin `start()` function exits (whichever comes
first), plugin's `stop()` function will be called, if defined.

IMPORTANT: `start()` function runs in a different thread than `stop()`
function. By the time `stop()` runs, depending on the circumstances,
`start()` thread may or may not exist.

IMPORTANT: `stop()` will always be called during shutdown, regardless of whether
start() exited with error, exited successfully or is still running.  `stop()`
must be able to deal with all 3 scenarios. The rationale for this design
decision is given Error Handling section.



### Persistence in Plugin Functions ###

While start() may persist, the other three functions (init(), stop() and
deinit()) must obviously not persist, since they run in the main thread.
Any blocking behavior exhibited in these functions (caused by a bug or
otherwise) will cause the entire application to hang, as will start() that
does not poll and/or honor is_running() flag.



### Returning Success/Failure from Plugin Function ###

Harness expects all four plugin functions (`init(), `start()`, `stop()` and
`deinit()`) to notify it in case of an error. This is done via function:

    set_error(PluginFuncEnv* env, ErrorType error, const char* format, ...);

Calling this function flags that the function has failed, and passes the
error type and string back to Harness. The converse is also true: not
calling this function prior to exiting the function implies success.
This distinction is important, because Harness may take certain actions
based on the status returned by each function.

IMPORTANT! Throwing exceptions from these functions is not supported.
If your plugin uses exceptions internally, that is fine, but please
ensure they are handled before reaching the Harness-Plugin boundary.


### Threading Concerns ###

For each plugin (independent of other plugins):
Of the 4 plugin functions, `init()` runs first. It is guaranteed that
it will exit before `start()` and `stop()` are called. `start()` and
`stop()` can be called in parallel to each other, in any order, with
their lifetimes possibly overlapping. They are guaranteed to both have
exited before `deinit()` is called.

If any of the 4 plugin functions spawn any additional threads, Harness
makes no provisions for interacting with them in any way: calling
Harness functions from them is not supported in particular; also such
threads should exit before their parent function finishes running.



### Error Handling ###

NOTE: WL#9558 HLD version of this section additionally discusses design,
      rationale for the approach chosen, etc; look there if interested.

When plugin functions encounter an error, they are expected to signal it via
set_error(). What happens next, depends on the case, but in all four
cases the error will be logged automatically by the harness. Also, the first
error passed from any plugin will be saved until the end of life-cycle
processing, then passed down to the code calling the harness. This will allow
the application code to deal with it accordingly (probably do some of its own
cleaning and shut down, but that's up to the application). In general, the first
error from init() or start() will cause the harness to initiate shut down
procedure, while the errors from stop() and deinit() will be ignored completely
(except of course for being logged and possibly saved for passing on at the
end). Actions taken for each plugin function are as follows:



#### init() fails:

  - skip init() for remaining plugins

  - don't run any start() and stop() (proceed directly to deinitialisation)

  - run deinit() only for plugins initialiased so far (excluding the failing
    one), in reverse order of initialisation, and exit

  - when init() is not provided (is null), it counts as if it ran, if it would
    have run before the failing plugin (according to topological order)



#### start() fails:

  - proceed to stop all plugins, then deinit() all in reverse order of
    initialisation and exit. Please note that ALL plugins will be flagged
    to stop and have their stop() function called (not just the ones that
    succeeded in starting - plugin's stop() must be able to deal with such
    a situation)



#### stop() or deinit() fails:

  - log error and ignore, proceed as if it didn't happen

*/

#ifndef MYSQL_HARNESS_LOADER_INCLUDED
#define MYSQL_HARNESS_LOADER_INCLUDED

#include "config_parser.h"
#include "filesystem.h"
#include "mysql/harness/dynamic_loader.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/plugin.h"

#include "harness_export.h"

#include "mpsc_queue.h"
#include "my_compiler.h"

#include <csignal>
#include <cstdarg>  // va_list
#include <exception>
#include <future>
#include <istream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

#ifdef FRIEND_TEST
class TestLoader;
#endif

namespace mysql_harness {

struct Plugin;
class Path;

/**
 * PluginFuncEnv object
 *
 * This object is the basis of all communication between Harness and plugin
 * functions. It is passed to plugin functions (as an opaque pointer), and
 * plugin functions return it back to Harness when calling Harness API
 * functions. It has several functions:
 *
 * - maintains a "running" flag, which controls starting/stopping
 *   of plugins
 *
 * - conveys exit status back to Harness after each plugin function exits
 *
 * - conveys more information (AppInfo, ConfigSection, ...) to
 *   plugin functions. Note that not all fields are set for all functions -
 *   setting ConfigSection ptr when calling init() makes no sense, for example
 *
 * @note Below we only briefly document the methods. For more information, see
 * Harness API documentation for their corresponding free-function proxies
 * in plugin.h
 */
class HARNESS_EXPORT PluginFuncEnv {
 public:
  /**
   * Constructor
   *
   * @param info AppInfo to pass to plugin function. Can be NULL.
   * Pointer is owned by the caller and must outlive plugin function call.
   * @param section ConfigSection to pass to plugin function. Can be NULL.
   * Pointer is owned by the caller and must outlive plugin function call.
   * @param running Set "running" flag. true = plugin should be running
   */
  PluginFuncEnv(const AppInfo *info, const ConfigSection *section,
                bool running = false);

  // further info getters
  // (see also corresponding Harness API functions in plugin.h for more info)
  const ConfigSection *get_config_section() const noexcept;
  const AppInfo *get_app_info() const noexcept;

  // running flag
  // (see also corresponding Harness API functions in plugin.h for more info)
  void set_running() noexcept;
  void clear_running() noexcept;
  bool is_running() const noexcept;
  bool wait_for_stop(
      uint32_t milliseconds) const noexcept;  // 0 = infinite wait

  // error handling
  // (see also corresponding Harness API functions in plugin.h for more info)
  bool exit_ok() const noexcept;
  MY_ATTRIBUTE((format(printf, 3, 0)))
  void set_error(ErrorType error_type, const char *fmt, va_list ap) noexcept;
  std::tuple<std::string, std::exception_ptr> pop_error() noexcept;

 private:
  const AppInfo *app_info_;              // \.
  const ConfigSection *config_section_;  //  > initialized in ctor
  bool running_;                         // /
  std::string error_message_;
  ErrorType error_type_ = kNoError;

  mutable std::condition_variable cond_;
  mutable std::mutex mutex_;
};

class HARNESS_EXPORT PluginThreads {
 public:
  void push_back(std::thread &&thr);

  // wait for the first non-fatal exit from plugin or all plugins exited
  // cleanly
  void try_stopped(std::exception_ptr &first_exc);

  void push_exit_status(std::exception_ptr &&eptr) {
    plugin_stopped_events_.push(std::move(eptr));
  }

  size_t running() const { return running_; }

  void wait_all_stopped(std::exception_ptr &first_exc);

  void join();

 private:
  std::vector<std::thread> threads_;
  size_t running_{0};

  /**
   * queue of events after plugin's start() function exited.
   *
   * nullptr if "finished without error", pointer to an exception otherwise
   */
  WaitingMPSCQueue<std::exception_ptr> plugin_stopped_events_;
};

class HARNESS_EXPORT Loader {
 public:
  /**
   * Constructor for Loader.
   *
   * @param program Name of our program
   * @param config Router configuration
   */
  Loader(std::string program, LoaderConfig &config)
      : config_(config), program_(std::move(program)) {}

  Loader(const Loader &) = delete;
  Loader &operator=(const Loader &) = delete;

  /**
   * Destructor.
   *
   * The destructor will call dlclose() on all unclosed shared
   * libraries.
   */

  ~Loader();

  /**
   * Fetch available plugins.
   *
   * @return List of names of available plugins.
   */

  std::list<Config::SectionKey> available() const;

  /**
   * Initialize and start all loaded plugins.
   *
   * All registered plugins will be initialized in proper order and
   * started (if they have a `start` callback).
   *
   * @throws first exception that was triggered by an error returned from any
   * plugin function.
   */
  void start();

  /**
   * Get reference to configuration object.
   *
   * @note In production code we initialize Loader with LoaderConfig
   * reference maintained by DIM, so this method will return this object.
   */
  LoaderConfig &get_config() { return config_; }

  /**
   * service names to wait on.
   *
   * add a service name and call on_service_ready() when the service ready().
   *
   * @see on_service_ready()
   */
  std::vector<std::string> &waitable_services() { return waitable_services_; }

  /**
   * service names to wait on.
   *
   * @see on_service_ready()
   */
  const std::vector<std::string> &waitable_services() const {
    return waitable_services_;
  }

  /**
   * set a function that's called after all plugins have been started.
   *
   * @see after_all_finished()
   */
  void after_all_started(std::function<void()> &&func) {
    after_all_started_ = std::move(func);
  }

  /**
   * set a function that's called after the first plugin exited.
   *
   * @see after_all_started()
   */
  void after_first_finished(std::function<void()> &&func) {
    after_first_finished_ = std::move(func);
  }

  /**
   * Register global configuration options supported by the application. Will be
   * used by the Loader to verify the [DEFAULT] options in the configuration.
   *
   * @param options array of global options supported by the applications
   */
  template <size_t N>
  void register_supported_app_options(
      const std::array<std::string_view, N> &options) {
    supported_app_options_.clear();
    for (const auto &option : options) {
      supported_app_options_.emplace_back(std::string(option));
    }
  }

 private:
  enum class Status { UNVISITED, ONGOING, VISITED };

  /**
   * Flags progress of Loader. The progress always proceeds from top to bottom
   * order in this list.
   */
  enum class Stage {
    // NOTE: do not alter order of these enums!
    Unset,
    Loading,
    Initializing,
    Starting,
    Running,
    Stopping,
    Deinitializing,
    Unloading,
  };

  /**
   * Load the named plugin from a specific library.
   *
   * @param plugin_name Name of the plugin to be loaded.
   *
   * @param library_name Name of the library the plugin should be
   * loaded from.
   *
   * @throws bad_plugin (std::runtime_error) on load error
   */
  const Plugin *load_from(const std::string &plugin_name,
                          const std::string &library_name);

  const Plugin *load(const std::string &plugin_name);

  /**
   * Load the named plugin and all dependent plugins.
   *
   * @param plugin_name Name of the plugin to be loaded.
   * @param key Key of the plugin to be loaded.
   *
   * @throws bad_plugin (std::runtime_error) on load error
   * @throws bad_section (std::runtime_error) when section 'plugin_name' is not
   * present in configuration
   *
   * @post After the execution of this procedure, the plugin and all
   * plugins required by that plugin will be loaded.
   */
  /** @overload */
  const Plugin *load(const std::string &plugin_name, const std::string &key);

  // IMPORTANT design note: start_all() will block until PluginFuncEnv objects
  // have been created for all plugins. This guarantees that the required
  // PluginFuncEnv will always exist when plugin stop() function is called.

  // start() calls these, indents reflect call hierarchy
  void load_all();  // throws bad_plugin on load error
  void setup_info();
  std::exception_ptr
  run();  // returns first exception returned from below harness functions
  std::exception_ptr init_all();  // returns first exception triggered by init()

  void
  start_all();  // forwards first exception triggered by start() to main_loop()

  std::exception_ptr
  main_loop();  // returns first exception triggered by start() or stop()

  // class stop_all() and waits for plugins the terminate
  std::exception_ptr stop_and_wait_all();

  std::exception_ptr stop_all();  // returns first exception triggered by stop()

  std::exception_ptr
  deinit_all();  // returns first exception triggered by deinit()

  void unload_all();
  size_t external_plugins_to_load_count();

  /**
   * Topological sort of all plugins and their dependencies.
   *
   * Will create a list of plugins in topological order from "top"
   * to "bottom".
   */
  bool topsort();
  bool visit(const std::string &name, std::map<std::string, Status> *seen,
             std::list<std::string> *order);

  /**
   * Holds plugin's API call information
   *
   * @note There's 1 instance per plugin type (not plugin instance)
   */
  class HARNESS_EXPORT PluginInfo {
   public:
    PluginInfo(const std::string &folder, const std::string &libname);
    PluginInfo(const Plugin *const plugin) : plugin_(plugin) {}

    void load_plugin_descriptor(const std::string &name);  // throws bad_plugin

    const Plugin *plugin() const { return plugin_; }

    const DynamicLibrary &library() const { return module_; }

   private:
    DynamicLibrary module_;
    const Plugin *plugin_{};
  };

  using PluginMap = std::map<std::string, PluginInfo>;

  // Init order is important, so keep config_ first.

  /**
   * Configuration sections for all plugins.
   */
  LoaderConfig &config_;

  /**
   * Map of all successfully-loaded plugins (without key name).
   */
  PluginMap plugins_;

  /**
   * Map of all {plugin instance -> plugin start() PluginFuncEnv} objects.
   * Initially these objects are created in Loader::start_all() and then kept
   * around until the end of Loader::stop_all(). At the time of writing,
   * PluginFuncEnv objects for remaining plugin functions (init(), stop() and
   * deinit()) are not required to live beyond their respective functions calls,
   * and are therefore created on stack (automatic variables) as needed during
   * those calls.
   */
  std::map<const ConfigSection *, std::shared_ptr<PluginFuncEnv>>
      plugin_start_env_;

  /**
   * active plugin threads.
   */
  PluginThreads plugin_threads_;

  /**
   * Initialization order.
   */
  std::list<std::string> order_;

  std::string logging_folder_;
  std::string plugin_folder_;
  std::string runtime_folder_;
  std::string config_folder_;
  std::string data_folder_;
  std::string program_;
  AppInfo appinfo_;

  void spawn_signal_handler_thread();

  std::mutex signal_thread_ready_m_;
  std::condition_variable signal_thread_ready_cond_;
  bool signal_thread_ready_{false};
  std::thread signal_thread_;

  std::vector<std::string> supported_app_options_;
  /**
   * Checks if all the options in the configuration fed to the Loader are
   * supported.
   *
   * @throws std::runtime_error if there is unsupported option in the
   * configuration
   */
  void check_config_options_supported();

  /**
   * Checks if all the options in the section [DEFAULT] in the configuration fed
   * to the Loader are supported.
   *
   * @throws std::runtime_error if there is unsupported option in the [DEFAULT]
   * section of the configuration
   */
  void check_default_config_options_supported();

  // service names that need to be waited on.
  //
  // @see on_service_ready()
  std::vector<std::string> waitable_services_;

  // called after "start_all()" succeeded.
  std::function<void()> after_all_started_;

  // called after "main_loop()" exited.
  std::function<void()> after_first_finished_;

#ifdef FRIEND_TEST
  friend class ::TestLoader;
#endif
};  // class Loader

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_LOADER_INCLUDED */
