MySQL Harness
=============

Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

Description
-----------

MySQL Harness is an extensible framework that handles loading and
unloading of *plugins*. The built-in features are dependency tracking
between plugins, configuration file handling, and support for plugin
life-cycles.


Building
--------

To build the MySQL Harness you use the standard steps to build from
CMake:

    cmake .
    make

If you want to do an out-of-source build, the procedure is:

    mkdir build
    cd build
    cmake <path-to-source>
    make


Building and running the unit tests
-----------------------------------

To build the unit tests:

    cmake <path-to-source> -DWITH_UNIT_TESTS=1

To run the tests

    make test


Coverage information
--------------------

To build so that coverage information is generated:

    cmake <path-to-source> -DENABLE_GCOV=1

To get coverage information, just run the program or the unit tests
(do not forget to enable the unit tests if you want to run them). Once
you have collected coverage information, you can generate an HTML
report in `<build-dir>/coverage/html` using:

    make coverage-html

There are three variables to control where the information is
collected and where the reports are written:

- `GCOV_BASE_DIR` is a cache variable with the full path to a base
  directory for the coverage information.

  It defaults to `${CMAKE_BUILD_DIR}/coverage`.
  
- `GCOV_INFO_FILE` is a cache varible with the full path to the info
  file for the collected coverage information.

  It defaults to `${GCOV_BASE_DIR}/coverage.info`.
  
- `GCOV_HTML_DIR` is a cache variable with the full path to the
  directory where the HTML coverage report will be generated.

  It defaults to `${GCOV_BASE_DIR}/html`.


Documentation
-------------

Documentation can be built as follows:

    make docs

The documentation is using Doxygen to extract documentation comments
from the source code.

The documentation will be placed in the `doc/` directory under the
build directory. For more detailed information about the code, please
read the documentation rather than rely on this `README`.


Installing
----------

To install the files, use `make install`. This will install the
harness, the harness library, the header files for writing plugins,
and the available plugins that were not marked with `NO_INSTALL` (see
below).

If you want to provide a different install prefix, you can do that by
setting the `CMAKE_INSTALL_PREFIX`:

    cmake . -DCMAKE_INSTALL_PREFIX=~/tmp


Running
-------

To start the harness, you need a configuration file. You can find an
example in `data/main.conf`:

    # Example configuration file

    [DEFAULT]
    logging_folder = /var/log/router
    config_folder = /etc/mysql/router
    plugin_folder = /var/lib/router
    runtime_folder = /var/run/router
    data_folder = /var/lib/router

    [example]
    library = example

The configuration file contain information about all the plugins that
should be loaded when starting and configuration options for each
plugin.  The default section contains configuration options available
in to all plugins.

To run the harness, just provide the configuration file as the only
argument:

    harness /etc/mysql/harness/main.conf

Note that the harness read directories for logging, configuration,
etc. from the configuration file so you have to make sure these are
present and that the section name is used to find the plugin structure
in the shared library (see below).

Typically, the harness will then load plugins from the directory
`/var/lib/harness` and write log files to `/var/log/harness`.


Writing Plugins
---------------

NOTE: This chapter quickly outlines the basic concepts, there is also
      more in-depth information available at the beginning of loader.h,
      which you will probably want to read to gain further insight.

All available plugins are in the `plugins/` directory. There is one
directory for each plugin and it is assumed that it contain a
`CMakeLists.txt` file.

The main `CMakeLists.txt` file provide an `add_harness_plugin`
function that can be used add new plugins.

    add_harness_plugin(<name> [ NO_INSTALL ]
                       INTERFACE <directory>
                       SOURCES <source> ...
                       DESTINATION <directory>
                       REQUIRES <plugin> ...)

This function adds a plugin named `<name>` built from the given
sources. If `NO_INSTALL` is provided, it will not be installed with
the harness (useful if you have plugins used for testing, see the
`tests/` directory). Otherwise, the plugin will be installed in the
directory specified by `DESTINATION`.

The header files in the directory given by `INTERFACE` are the
interface files to the plugin and shall be used by other plugins
requiring features from this plugin. These header files will be
installed alongside the harness include files and will also be made
available to other plugins while building from source.

### Plugin Directory Structure ###

Similar to the harness, each plugin have two types of files:

* Plugin-internal files used to build the plugin. These include the
  source files and but also header files associated with each source
  file and are stored in the `src` directory of the plugin directory.
* Interface files used by other plugins. These are header files that
  are made available to other plugins and are installed alongside the
  harness installed files, usually under the directory
  `/usr/include/mysql/harness`.

### Application Information Structure ###

The application information structure contains some basic fields
providing information to the plugin. Currently these fields are
provided:

    struct AppInfo {
      const char *program;                 /* Name of the application */
      const char *plugin_folder;           /* Location of plugins */
      const char *logging_folder;          /* Log file directory */
      const char *config_folder;           /* Config file directory */
      const char *runtime_folder;          /* Run file directory */
      const Config* config;                /* Configuration information */
    };


### Configuration Section Information Structure ###

Configuration section object (class `ConfigSection`) carries configuration
information specific to a particular plugin instance. It reflects
information provided in router configuration file for one specific
configuration section. Only parts relevant to configuration retrieval are
listed below, the actual class carries additional methods and fields:

    class ConfigSection {
     public:
      ConfigSection& operator=(const ConfigSection&) = delete;
      std::string    get(const std::string& option) const;

     public:
      const std::string name;
      const std::string key;
    };


### Plugin Structure ###

To define a new plugin, you have to create an instance of the
`Plugin` structure in your plugin similar to this:

    #include <mysql/harness/plugin.h>

    static const char* requires[] = {
      "magic (>>1.0)",
    };

    Plugin example = {
      PLUGIN_ABI_VERSION,
      ARCHITECTURE_DESCRIPTOR,
      "An example plugin",       // Brief description of plugin
      VERSION_NUMBER(1,0,0),     // Version number of the plugin

      // Array of required plugins
      sizeof(requires)/sizeof(*requires),
      requires,

      // Array of plugins that conflict with this one
      0,
      NULL,

      // pointers to API functions, can be NULL if not implemented
      init,
      deinit,
      start,
      stop,
    };


### Initialization and Cleanup ###

After the plugin is loaded, the `init()` function is called for all
plugins with a pointer to the function call environment object (not to be
confused with application environment passed from the shell) as the only argument.

The `init()` functions are called in dependency order so that all
`init()` functions in required plugins are called before the `init()`
function in the plugin itself.

Before the harness exits, it will call the `deinit()` function with a
pointer to environment object as the only argument.


### Starting and Stopping the Plugin ###

After all the plugins have been successfully initialized, a thread
will be created for each plugin that has a `start()` function
defined.

#### Overview ####

The `start()` function will be called with a pointer to environment object
as the only parameter. When all the plugins return from their `start()`
functions, the harness will perform cleanup and exit. If plugin
provides a `stop()` function, it will also be called at that time.

IMPORTANT: `start()` function runs in a different thread than `stop()`
function. By the time `stop()` runs, depending on the circumstances,
`start()` thread may or may not exist.

#### Shutdown Provision ####
It is typical to implement start function in such a way that it will
"persist" (i.e. it will run some forever-loop processing requests
rather than exit briefly after being called). In such case, Harness
must have a way to terminate it during shutdown operation. This is
accomplished by providing a `is_running()` function, that polls harness
state. This function should be routinely called from plugin's `start()`
function to determine if it should shut down. Typically, `start()`
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
should that be better suited for your plugin. Instead of quickly returning
a boolean flag, it will block until Harness initiates shut down. It is
functionally equivalent to:

    while (is_running())
    {
      // sleep a little or break on timeout
    }

When entering shutdown mode, Harness will notify all plugins to shut down
via mechanism described above. It is also possible for plugins to exit on
their own, whether due to error or intended behavior. In some designs,
`start()` function might need to be able to set the flag returned by
`is_running()` to false. For such cases, `clear_running()` flag is provided,
which will do exactly that.

IMPORTANT! Please note that all 3 functions described above (`is_running()`,
`wait_for_stop()` and `clear_running()`) can only be called from a thread
running `start()` function. If your plugins spawns more threads, these
functions CANNOT be called from them.

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


License
-------

License information can be found in the License.txt file.

This distribution may include materials developed by third
parties. For license and attribution notices for these
materials, please refer to the documentation that accompanies
this distribution (see the "Licenses for Third-Party Components"
appendix) or view the online documentation at
<http://dev.mysql.com/doc/>.

GPLv2 Disclaimer
For the avoidance of doubt, except that if any license choice
other than GPL or LGPL is available it will apply instead,
Oracle elects to use only the General Public License version 2
(GPLv2) at this time for any software where a choice of GPL
license versions is made available with the language indicating
that GPLv2 or any later version may be used, or where a choice
of which version of the GPL is applied is otherwise unspecified.
