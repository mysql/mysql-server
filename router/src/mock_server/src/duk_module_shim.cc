/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "duk_module_shim.h"

#include <cstdlib>  // _fullpath
#include <cstring>

#ifdef _WIN32
#include <direct.h>  // getcwd
#include <io.h>      // write
#else
#include <unistd.h>
#endif
#include <sys/stat.h>

#include "duk_module_duktape.h"
#include "duk_module_node.h"
#include "duk_node_fs.h"
#include "duktape.h"

#ifndef PATH_MAX
#ifdef _MAX_PATH
// windows has MAX_PATH instead
#define PATH_MAX _MAX_PATH
#endif
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

static duk_ret_t node_path_join(duk_context *ctx) {
  duk_idx_t arg_count = duk_get_top(ctx);
  duk_idx_t i;

  duk_push_string(ctx, "/");
  for (i = 0; i < arg_count; i++) {
    duk_dup(ctx, i);
  }
  duk_join(ctx, arg_count);

  return 1;
}

// normalize path
static duk_ret_t normalize_path(duk_context *ctx, duk_idx_t obj_idx) {
  const char *p = duk_require_string(ctx, obj_idx);

#ifdef _WIN32
  char resolved_path[_MAX_PATH];
  duk_push_string(ctx, _fullpath(resolved_path, p, sizeof(resolved_path)));
#else
  char resolved_path[PATH_MAX];

  duk_push_string(ctx, realpath(p, resolved_path));
#endif
  return 1;
}

static duk_ret_t node_format_string(duk_context *ctx) {
  duk_idx_t arg_count = duk_get_top(ctx);
  duk_idx_t arg_ndx = 1;
  size_t section_count = 0;

  duk_push_lstring(ctx, "", 0);
  if (arg_count > 0) {
    const char *fmt = duk_require_string(ctx, 0);
    const char *start = nullptr;

    const char *s = fmt;
    for (; s < fmt + strlen(fmt); s++) {
      if (*s == '%') {
        if (start) {
          duk_push_lstring(ctx, start, s - start);
          section_count++;

          start = nullptr;
        }
        switch (*(s + 1)) {
          case '%':
            duk_push_lstring(ctx, "%", 1);
            section_count++;
            s++;
            break;
          case 's':
          case 'd':
          case 'i':
          case 'j':  // TODO: json
          case 'f':
            duk_dup(ctx, arg_ndx++);
            duk_safe_to_string(ctx, -1);
            section_count++;
            s++;
            break;
          default:
            s++;
            fprintf(stderr, "unknown format op: %c", *(s + 1));
        }
      } else {
        if (nullptr == start) start = s;
      }
    }
    if (start) {
      duk_push_lstring(ctx, start, s - start);
      section_count++;
    }
  }

  duk_join(ctx, section_count);

  return 1;
}

static duk_ret_t node_path_resolve(duk_context *ctx) {
  normalize_path(ctx, 0);
  return 1;
}

static const duk_function_list_entry path_module_funcs[] = {
    {"join", node_path_join, DUK_VARARGS},
    {"resolve", node_path_resolve, 1},
    {nullptr, nullptr, 0}};

static duk_ret_t node_util_inherits(duk_context *ctx) {
  if (DUK_EXEC_SUCCESS !=
      duk_pcompile_string(
          ctx, DUK_COMPILE_FUNCTION,
          "function inherits(ctor, superCtor) {\n"
          "  ctor.super_ = superCtor\n"
          "  Object.setPrototypeOf(ctor.prototype, superCtor.prototype);\n"
          "});\n")) {
    return duk_throw(ctx);
  }
  duk_dup(ctx, 0);
  duk_dup(ctx, 1);
  if (DUK_EXEC_SUCCESS != duk_pcall(ctx, 2)) {
    return duk_throw(ctx);
  }

  return 1;
}

static const duk_function_list_entry util_module_funcs[] = {
    {"inherits", node_util_inherits, 2},

    {nullptr, nullptr, 0}};

static const duk_function_list_entry fs_module_funcs[] = {
    {"readSync", duk_node_fs_read_file_sync, 1},

    {nullptr, nullptr, 0}};

/*
static const duk_function_list_entry events_module_funcs[] = {
  { nullptr, nullptr, 0 }
};
*/

static const duk_function_list_entry assert_module_funcs[] = {
    {nullptr, nullptr, 0}};

static const duk_function_list_entry os_module_funcs[] = {
    {nullptr, nullptr, 0}};

static duk_ret_t node_console_log(duk_context *ctx) {
  // duk_push_lstring(ctx, "", 0);
  // duk_push_string(ctx, "WARN: ");
  node_format_string(ctx);
  // duk_join(ctx, 2);

  puts(duk_get_string(ctx, -1));

  // duk_pop(ctx);

  return 0;
}

static const duk_function_list_entry console_module_funcs[] = {
    {"log", node_console_log, DUK_VARARGS},
    {"warn", node_console_log, DUK_VARARGS},

    {nullptr, nullptr, 0}};

static duk_ret_t node_tty_isatty(duk_context *ctx) {
#ifndef _WIN32
  duk_push_boolean(ctx, isatty(duk_require_int(ctx, 0)));
#else
  duk_push_boolean(ctx, 0);
#endif
  return 1;
}

static duk_ret_t node_tty_getwindowsize(duk_context *ctx) {
  duk_push_array(ctx);
  duk_push_int(ctx, 25);
  duk_put_prop_index(ctx, -2, 0);
  duk_push_int(ctx, 80);
  duk_put_prop_index(ctx, -2, 1);
  return 1;
}

static const duk_function_list_entry tty_module_funcs[] = {
    {"isatty", node_tty_isatty, 1},
    {"getWindowSize", node_tty_getwindowsize, 0},
    {nullptr, nullptr, 0}};

static duk_ret_t node_process_getenv(duk_context *ctx) {
  duk_push_string(ctx, getenv(duk_require_string(ctx, 0)));
  return 1;
}

static duk_ret_t node_process_cwd(duk_context *ctx) {
  char current_dir[PATH_MAX];
  duk_push_string(ctx, getcwd(current_dir, sizeof(current_dir)));
  return 1;
}

static duk_ret_t node_process_on(duk_context *ctx) {
  (void)ctx;
  return 0;
}

static duk_ret_t node_process_remove_listener(duk_context *ctx) {
  (void)ctx;
  return 0;
}

static duk_ret_t node_process_nexttick(duk_context *ctx) {
  duk_require_function(ctx, 0);

  duk_dup(ctx, 0);
  duk_pcall(ctx, 0);
  return 0;
}

static const duk_function_list_entry process_module_funcs[] = {
    {"getenv", node_process_getenv, 1},
    {"cwd", node_process_cwd, 0},
    {"on", node_process_on, 2},
    {"removeListener", node_process_remove_listener, 2},
    {"nextTick", node_process_nexttick, 1},
    {nullptr, nullptr, 0}};

static duk_ret_t node_write_stderr(duk_context *ctx) {
  size_t buf_len;
  const char *buf = duk_require_lstring(ctx, 0, &buf_len);
  if (write(STDERR_FILENO, buf, buf_len) < 0) {
    // just to avoid the compiler warning about ignoring retval
  }

  return 0;
}

static duk_ret_t node_write_stdout(duk_context *ctx) {
  size_t buf_len;
  const char *buf = duk_require_lstring(ctx, 0, &buf_len);
  if (write(STDOUT_FILENO, buf, buf_len) < 0) {
    // just to avoid the compiler warning about ignoring retval
  }

  return 0;
}

static duk_ret_t node_clear_timeout(duk_context *ctx) {
  (void)ctx;
  return 0;
}

static duk_ret_t dukopen_process_module(duk_context *ctx) {
  duk_push_object(ctx);

  duk_put_function_list(ctx, -1, process_module_funcs);

  duk_push_object(ctx);
  duk_push_int(ctx, STDERR_FILENO);
  duk_put_prop_string(ctx, -2, "fd");

  duk_push_c_function(ctx, node_write_stderr, 1);
  duk_put_prop_string(ctx, -2, "write");

  duk_put_prop_string(ctx, -2, "stderr");

  duk_push_object(ctx);
  duk_push_int(ctx, STDOUT_FILENO);
  duk_put_prop_string(ctx, -2, "fd");

  duk_push_c_function(ctx, node_write_stdout, 1);
  duk_put_prop_string(ctx, -2, "write");

  duk_put_prop_string(ctx, -2, "stdout");

  return 1;
}

static duk_ret_t dukopen_process_module_init_env(duk_context *ctx) {
  duk_get_global_string(ctx, "process");
  if (DUK_EXEC_SUCCESS !=
      duk_pcompile_string(ctx, DUK_COMPILE_FUNCTION,
                          "function () {\n"
                          "  return new Proxy({}, {\n"
                          "    get: function(targ, key, recv) {\n"
                          "        return process.getenv(key);\n"
                          "      }\n"
                          "  });\n"
                          "}")) {
    return duk_throw(ctx);
  }
  if (DUK_EXEC_SUCCESS != duk_pcall(ctx, 0)) {
    return duk_throw(ctx);
  }

  duk_put_prop_string(ctx, -2, "env");

  duk_push_array(ctx);
  duk_put_prop_string(ctx, -2, "argv");

  duk_pop(ctx);

  return 0;
}

static duk_ret_t cb_resolve_module(duk_context *ctx) {
  // expect module.paths in the global scope
  // append .js, check if it exits
  // if file is a directory:
  //   check for package.js -> "main"
  //   otherwise index.js
  const char *module_id = duk_require_string(ctx, 0);
  const char *parent_id = duk_require_string(ctx, 1);

  // fprintf(stderr, "%s:%d: %s\n", __PRETTY_FUNCTION__, __LINE__, module_id);
  if ((0 == strcmp(module_id, "path")) || (0 == strcmp(module_id, "util")) ||
      (0 == strcmp(module_id, "events")) ||
      (0 == strcmp(module_id, "assert")) || (0 == strcmp(module_id, "tty")) ||
      (0 == strcmp(module_id, "os")) || (0 == strcmp(module_id, "fs"))) {
    duk_push_string(ctx, module_id);
    return 1;
  }

  if (module_id[0] == '/') {
    struct stat st;
    if ((0 == stat(module_id, &st)) && (st.st_mode & S_IFMT) == S_IFREG) {
      // file exists, leave it on the stack
      duk_push_string(ctx, module_id);

      normalize_path(ctx, -1);
      duk_remove(ctx, -2);

      return 1;
    }
  } else {
    int has_parent_id = strlen(parent_id) > 0;

    if (has_parent_id && module_id[0] == '.') {
      // strip the filename from the parent's id
      const char *last_sep = strrchr(parent_id, '/');
      if (nullptr == last_sep) {
        return duk_generic_error(ctx, "expected / in %s", parent_id);
      }
      // get the prefix of the string as base dir
      duk_push_array(ctx);
      duk_push_lstring(ctx, parent_id, last_sep - parent_id);
      duk_put_prop_index(ctx, -2, 0);
    } else {
      duk_get_global_string(ctx, "module");
      duk_get_prop_string(ctx, -1, "paths");
      duk_remove(ctx, -2);  // we don't need 'module' anymore
    }
    duk_enum(ctx, -1, DUK_ENUM_ARRAY_INDICES_ONLY);
    duk_remove(ctx, -2);  // we don't need 'paths' anymore

    while (duk_next(ctx, -1, 1)) {
      // walk the paths
      const char *search_dir = duk_require_string(ctx, -1);

      // has <module-id>?
      duk_push_string(ctx, "/");  // sep
      duk_push_string(ctx, search_dir);
      duk_push_string(ctx, module_id);  // module_id
      duk_join(ctx, 2);

      struct stat st;
      if ((0 == stat(duk_get_string(ctx, -1), &st)) &&
          (st.st_mode & S_IFMT) == S_IFREG) {
        // file exists, leave it on the stack
        duk_remove(ctx, -2);  // remove value
        duk_remove(ctx, -2);  // remove key
        duk_remove(ctx, -2);  // remove enum

        normalize_path(ctx, -1);
        duk_remove(ctx, -2);

        return 1;
      }

      duk_pop(ctx);  // filename

      // has <module-id>.js?
      duk_push_string(ctx, "/");  // sep
      duk_push_string(ctx, search_dir);

      {
        duk_push_string(ctx, "");         // sep
        duk_push_string(ctx, module_id);  // module_id
        duk_push_string(ctx, ".js");
        duk_join(ctx, 2);
      }

      duk_join(ctx, 2);

      if ((0 == stat(duk_get_string(ctx, -1), &st)) &&
          (st.st_mode & S_IFMT) == S_IFREG) {
        // file exists, leave it on the stack
        duk_remove(ctx, -2);  // remove value
        duk_remove(ctx, -2);  // remove key
        duk_remove(ctx, -2);  // remove enum

        normalize_path(ctx, -1);
        duk_remove(ctx, -2);

        return 1;
      }

      duk_pop(ctx);  // filename

      // has <module-id>/package.json?
      duk_push_string(ctx, "/");  // sep
      duk_push_string(ctx, search_dir);
      duk_push_string(ctx, module_id);  // module_id
      duk_push_string(ctx, "package.json");

      duk_join(ctx, 3);

      if ((0 == stat(duk_get_string(ctx, -1), &st)) &&
          (st.st_mode & S_IFMT) == S_IFREG) {
        // file exists, leave it on the stack
        // eval it and loop for "main"

        duk_push_c_function(ctx, duk_node_fs_read_file_sync, 1);
        duk_dup(ctx, -2);  // the path
        if (DUK_EXEC_SUCCESS != duk_pcall(ctx, 1)) {
          // file existed, but now we failed to open it
          return duk_throw(ctx);  // rethrow the error
        }
        // we get a buffer, but want to return a string
        duk_buffer_to_string(ctx, -1);
        duk_json_decode(ctx, -1);

        // we should have an object on the stack now
        if (!duk_is_object(ctx, -1)) {
          return duk_generic_error(ctx, "expected an object in %s",
                                   duk_get_string(ctx, -2));
        }

        duk_get_prop_string(ctx, -1, "main");
        duk_remove(ctx, -2);  // remove json-object
        if (duk_is_string(ctx, -1)) {
          const char *main_file = duk_require_string(ctx, -1);

          duk_push_string(ctx, "/");  // sep
          duk_push_string(ctx, search_dir);
          duk_push_string(ctx, module_id);  // module_id
          duk_push_string(ctx, main_file);

          duk_join(ctx, 3);

          if ((0 == stat(duk_get_string(ctx, -1), &st)) &&
              (st.st_mode & S_IFMT) == S_IFREG) {
            duk_remove(ctx, -2);  // remove old filename
            duk_remove(ctx, -2);  // remove value
            duk_remove(ctx, -2);  // remove key
            duk_remove(ctx, -2);  // remove enum

            normalize_path(ctx, -1);
            duk_remove(ctx, -2);

            return 1;
          }
          duk_pop(ctx);  // new filename

          duk_push_string(ctx, "/");  // sep
          duk_push_string(ctx, search_dir);
          duk_push_string(ctx, module_id);  // module_id
          duk_push_string(ctx, main_file);
          duk_push_string(ctx, "index.js");

          duk_join(ctx, 4);

          if ((0 == stat(duk_get_string(ctx, -1), &st)) &&
              (st.st_mode & S_IFMT) == S_IFREG) {
            duk_remove(ctx, -2);  // remove old filename
            duk_remove(ctx, -2);  // remove value
            duk_remove(ctx, -2);  // remove key
            duk_remove(ctx, -2);  // remove enum

            normalize_path(ctx, -1);
            duk_remove(ctx, -2);

            return 1;
          }
          duk_pop(ctx);  // new filename

          duk_pop(ctx);  // main-file
        } else {
          // no main file set
          duk_pop(ctx);  // main-file
        }
      }

      duk_pop(ctx);  // filename

      // has <module-id>/index.js?
      duk_push_string(ctx, "/");        // sep
      duk_dup(ctx, -2);                 // value
      duk_push_string(ctx, module_id);  // module_id
      duk_push_string(ctx, "index.js");

      duk_join(ctx, 3);

      if ((0 == stat(duk_get_string(ctx, -1), &st)) &&
          (st.st_mode & S_IFMT) == S_IFREG) {
        // file exists, leave it on the stack
        duk_remove(ctx, -2);  // remove value
        duk_remove(ctx, -2);  // remove key
        duk_remove(ctx, -2);  // remove enum

        normalize_path(ctx, -1);
        duk_remove(ctx, -2);

        return 1;
      }

      duk_pop(ctx);  // filename

      duk_pop(ctx);  // value
      duk_pop(ctx);  // key
    }

    duk_pop(ctx);  // enum
  }

  return duk_generic_error(ctx, "Cannot find module: %s", module_id);
}

static duk_ret_t cb_load_module(duk_context *ctx) {
  // 0 resolved_id
  // 1 exports
  // 2 module
  //
  duk_push_array(ctx);
  duk_put_prop_string(ctx, 2, "paths");

  const char *resolved_id = duk_require_string(ctx, 0);

  if (0 == strcmp(resolved_id, "path")) {
    duk_put_function_list(ctx, 1, path_module_funcs);

    duk_push_undefined(ctx);
    return 1;
  } else if (0 == strcmp(resolved_id, "util")) {
    duk_put_function_list(ctx, 1, util_module_funcs);

    duk_push_undefined(ctx);
    return 1;
  } else if (0 == strcmp(resolved_id, "fs")) {
    duk_put_function_list(ctx, 1, fs_module_funcs);

    duk_push_undefined(ctx);
    return 1;
  } else if (0 == strcmp(resolved_id, "process")) {
    duk_put_function_list(ctx, 1, process_module_funcs);

    duk_push_undefined(ctx);
    return 1;
  } else if (0 == strcmp(resolved_id, "console")) {
    duk_put_function_list(ctx, 1, console_module_funcs);

    duk_push_undefined(ctx);
    return 1;
  } else if (0 == strcmp(resolved_id, "events")) {
    duk_push_string(ctx, "events.js");
    if (DUK_EXEC_SUCCESS !=
        duk_pcompile_string_filename(
            ctx, DUK_COMPILE_EVAL,
            "function EventEmitter() {\n"
            "  EventEmitter.init.call(this);\n"
            "};\n"
            "EventEmitter.prototype._events = undefined;\n"
            "EventEmitter.prototype.on = function(name, cb) {\n"
            "  if (this._events === undefined) {\n"
            "    this._events = Object.create(null);\n"
            "  }\n"
            "  if (!(name in this._events)) {\n"
            "    this._events[name] = [];\n"
            "  }\n"
            "  this._events[name].push(cb);\n"
            "};\n"
            "EventEmitter.init = function() {\n"
            "  if (this._events === undefined || \n"
            "      this._events == Object.getPropertyOf(this)._events) {\n"
            "    this._events = Object.create(null);\n"
            "  }\n"
            "};\n"
            "EventEmitter.prototype.once = function(name, cb) {\n"
            "  if (this._events === undefined) {\n"
            "    this._events = Object.create(null);\n"
            "  }\n"
            "  if (!(name in this._events)) {\n"
            "    this._events[name] = [];\n"
            "  }\n"
            "  this._events[name].push(cb);\n"
            "};\n"
            "EventEmitter.prototype.emit = function(typ) {\n"
            "  var args = Array.prototype.slice.call(arguments, 1);\n"
            "  if (this._events === undefined) {\n"
            "    return false;\n"
            "  }\n"
            "  if (!(typ in this._events)) {\n"
            "    return false;\n"
            "  }\n"
            "  var handlers = this._events[typ];\n"
            "  if (handlers === undefined) {\n"
            "    return false;\n"
            "  }\n"
            "  for (var ndx = 0; ndx < handlers.length; ndx++) {\n"
            "    Reflect.apply(handlers[ndx], this, args);"
            "  }\n"
            "  return true;\n"
            "};\n"
            // make sure eval returns it
            "EventEmitter;\n")) {
      return duk_throw(ctx);
    }
    if (DUK_EXEC_SUCCESS != duk_pcall(ctx, 0)) {
      return duk_throw(ctx);
    }
    duk_put_prop_string(ctx, 1, "EventEmitter");

    duk_push_undefined(ctx);
    return 1;
  } else if (0 == strcmp(resolved_id, "assert")) {
    duk_put_function_list(ctx, 1, assert_module_funcs);

    duk_push_undefined(ctx);
    return 1;
  } else if (0 == strcmp(resolved_id, "tty")) {
    duk_put_function_list(ctx, 1, tty_module_funcs);

    duk_push_undefined(ctx);
    return 1;
  } else if (0 == strcmp(resolved_id, "os")) {
    duk_put_function_list(ctx, 1, os_module_funcs);

    duk_push_undefined(ctx);
    return 1;
  }

  duk_push_c_function(ctx, duk_node_fs_read_file_sync, 1);
  duk_push_string(ctx, resolved_id);
  if (DUK_EXEC_SUCCESS != duk_pcall(ctx, 1)) {
    // file existed, but now we failed to open it
    return duk_throw(ctx);  // rethrow the error
  }

  // we get a buffer, but want to return a string
  duk_buffer_to_string(ctx, -1);

  return 1;
}

void duk_module_shim_init(duk_context *ctx,
                          const std::vector<std::string> &prefixes) {
  // init the basic node-js builtins
  duk_push_c_function(ctx, dukopen_process_module, 0);
  duk_call(ctx, 0);
  duk_put_global_string(ctx, "process");

  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, console_module_funcs);
  duk_put_global_string(ctx, "console");

  dukopen_process_module_init_env(ctx);

  duk_push_c_function(ctx, node_clear_timeout, 1);
  duk_put_global_string(ctx, "clearTimeout");

  // var _module = []
  duk_idx_t module_ndx = duk_push_object(ctx);  // module

  // var _paths = []
  duk_idx_t paths_ndx = duk_push_array(ctx);  // array

  size_t ndx{};
  for (const auto &prefix : prefixes) {
    // var _path = path.join(cwd, "local_modules");
    duk_push_c_function(ctx, node_path_join, DUK_VARARGS);

    duk_push_string(ctx, prefix.c_str());
    duk_push_string(ctx, "local_modules");

    duk_pcall(ctx, 2);

    // _paths[0] = _path
    duk_put_prop_index(ctx, paths_ndx, ndx++);

    // var _path = path.join(cwd, "npm", "node_modules");
    duk_push_c_function(ctx, node_path_join, DUK_VARARGS);

    duk_push_string(ctx, prefix.c_str());
    duk_push_string(ctx, "npm");
    duk_push_string(ctx, "node_modules");

    duk_pcall(ctx, 3);

    // _paths[1] = _path
    duk_put_prop_index(ctx, paths_ndx, ndx++);
  }

  // _modules["paths"] = _paths
  duk_put_prop_string(ctx, module_ndx, "paths");

  // modules = _modules
  duk_put_global_string(ctx, "module");

  // as the module.paths is setup, init the module loader

  duk_push_object(ctx);
  duk_push_c_function(ctx, cb_resolve_module, DUK_VARARGS);
  duk_put_prop_string(ctx, -2, "resolve");
  duk_push_c_function(ctx, cb_load_module, DUK_VARARGS);
  duk_put_prop_string(ctx, -2, "load");
  duk_module_node_init(ctx);
}

// duktape-cli only knows about the 'duk_module_duktape_init()' call
// provide a facade which looks the same, but calls the node-js compat
// loader
void duk_module_duktape_init(duk_context *ctx) {
  // var cwd = getcwd()
  duk_push_c_function(ctx, node_process_cwd, 0);
  duk_pcall(ctx, 0);

  duk_module_shim_init(ctx, std::vector<std::string>{duk_get_string(ctx, -1)});
  duk_pop(ctx);
}
