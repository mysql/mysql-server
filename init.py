# Copyright (c) 2020, 2021, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
# This program is distributed in the hope that it will be useful,  but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
"""Plugin registration

This file is automatically loaded by the MySQL Shell at startup time.

It registers the plugin objects and then imports all sub-modules to
register the plugin object member functions.
"""

from mysqlsh.plugin_manager import plugin, plugin_function, VERSION

# Create a class representing the structure of the plugin and use the
# @register_plugin decorator to register it
@plugin
class plugins:
    """Plugin to manage MySQL Shell plugins

    This global object exposes a list of shell extensions
    to manage MySQL Shell plugins

    Use plugins.about() to get more information about writing
    MySQL Shell plugins.
    """
    pass

    class repositories:
        """
        Manages the registry of plugin repositories.
        """
        def __init__(self):
            from mysqlsh.plugin_manager import repositories

@plugin_function("plugins.info")
def info():
    """Prints basic information about the plugin manager.

    Returns:
        None
    """
    print(f"MySQL Shell Plugin Manager Version {VERSION}")


@plugin_function("plugins.version")
def version():
    """Returns the version number of the plugin manager.

    Returns:
        str
    """
    return VERSION


@plugin_function("plugins.about")
def info():
    """Prints detailed information about the MySQL Shell plugin support.

    Returns:
        None
    """
    print(
        """
The MySQL Shell allows extending its base functionality through the creation
of plugins.

A plugin is a folder containing the code that provides the functionality to
be made available on the MySQL Shell.

User defined plugins should be located at plugins folder at the following
paths:

- Windows: %AppData%\\MySQL\\mysqlsh\\plugins
- Others: ~/.mysqlsh/plugins

A plugin must contain an init file which is the entry point to load the
extension:

- init.js for plugins written in JavaScript.
- init.py for plugins written in Python.

On startup, the shell traverses the folders inside of the *plugins* folder
searching for the plugin init file. The init file will be loaded on the
corresponding context (JavaScript or Python).

Use Cases

The main use cases for MySQL Shell plugins include:

- Definition of shell reports to be used with the \\show and \\watch Shell
Commands.
- Definition of new Global Objects with user defined functionality.

For additional information on shell reports execute: \\? reports

For additional information on extension objects execute: \\? extension objects
"""
    )


@plugin_function("plugins.list")
def list_plugins(**kwargs):
    """Lists all available MySQL Shell plugins.

    This function will list all all available plugins in the registered
    plugin repositories. To add a new plugin repository use the
    plugins.repositories.add() function.

    Args:
        **kwargs: Optional parameters

    Keyword Args:
        return_formatted (bool): If set to true, a list object is returned.
        interactive (bool): Whether user input is accepted

    Returns:
        None or a list of dicts representing the plugins
    """
    import mysqlsh.plugin_manager.plugins as _plugins

    return _plugins.list_plugins(**kwargs)


@plugin_function("plugins.install")
def install_plugin(name=None, **kwargs):
    """Installs a MySQL Shell plugin.

    This function download and install a plugin

    Args:
        name (str): The name of the plugin.
        **kwargs: Optional parameters

    Keyword Args:
        version (str): If specified, that specific version of the plugin will
            be installed
        force_install (bool): It set to true will first remove the plugin
            if it already exists
        return_object (bool): Whether to return the object
        interactive (bool): Whether user input is accepted
        printouts (bool): Whether information should be printed
        raise_exceptions (bool):  Whether exceptions are raised

    Returns:
        None or plugin information
    """
    import mysqlsh.plugin_manager.plugins as _plugins

    return _plugins.install_plugin(name, **kwargs)


@plugin_function("plugins.uninstall")
def uninstall_plugin(name=None, **kwargs):
    """Uninstalls a MySQL Shell plugin.

    This function uninstall a plugin

    Args:
        name (str): The name of the plugin.
        **kwargs: Optional parameters

    Keyword Args:
        interactive (bool): Whether user input is accepted
        printouts (bool): Whether information should be printed
        raise_exceptions (bool):  Whether exceptions are raised

    Returns:
        None or plugin information
    """
    import mysqlsh.plugin_manager.plugins as _plugins

    return _plugins.uninstall_plugin(name, **kwargs)


@plugin_function("plugins.update")
def update_plugin(name=None, **kwargs):
    """Updates MySQL Shell plugins.

    This function updates on or all plugins

    Args:
        name (str): The name of the plugin.
        **kwargs: Optional parameters

    Keyword Args:
        interactive (bool): Whether user input is accepted
        raise_exceptions (bool):  Whether exceptions are raised

    Returns:
        None or plugin information
    """
    import mysqlsh.plugin_manager.plugins as _plugins

    return _plugins.update_plugin(name, **kwargs)


@plugin_function("plugins.details")
def plugin_details(name=None, **kwargs):
    """Gives detailed information about a MySQL Shell plugin.

    Args:
        name (str): The name of the plugin.
        **kwargs: Optional parameters

    Keyword Args:
        interactive (bool): Whether user input is accepted

    Returns:
        None or plugin information
    """
    import mysqlsh.plugin_manager.plugins as _plugins

    return _plugins.plugin_details(name, **kwargs)
