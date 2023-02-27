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

import json
import os
import re
import shutil
import zipfile

import mysqlsh

from enum import Enum

from . import general
from . import repositories
from mysqlsh.plugin_manager import validate_shell_version


class Filter(Enum):
    NONE = "all plugins"
    ONLY_INSTALLED = "installed plugins"
    NOT_INSTALLED = "available plugins for installation"
    UPDATABLE = "updatable plugins"


def parse_version(version):
    ret_val = tuple(int(x) if x.isnumeric() else x for x in version.split("."))
    return ret_val


def get_repositories_with_plugins(plugin_filter=Filter.NONE,
                                  printouts=True,
                                  raise_exceptions=False):
    """Downloads all plugin information from the available repositories

    Args:
        printouts (bool): Whether to print out information
        raise_exceptions (bool): Whether exceptions should be raised

    Returns:
        A list of repositories including their plugins
    """
    if printouts:
        print(f"Fetching list of {plugin_filter.value}...\n")

    # Get the list of all repositories, cSpell:ignore repos
    repos = repositories.get_plugin_repositories(return_formatted=False)

    try:
        i = 1
        for r in repos:
            # If the manifest is stored in a zip, extact that zip and get the
            # manifest file
            if r.get("url").endswith(".zip"):
                # Download the zip file
                zip_file_path = general.get_shell_temp_dir("manifest.zip")
                # If there is an earlier manifest.zip, remove it
                if os.path.isfile(zip_file_path):
                    os.remove(zip_file_path)
                if not general.download_file(r.get("url"), zip_file_path):
                    continue
                # Extract the zip file
                manifest_dir = general.get_shell_user_dir(
                    "temp", "manifest_zip")
                if os.path.isdir(manifest_dir):
                    shutil.rmtree(manifest_dir)
                try:
                    with zipfile.ZipFile(zip_file_path, 'r') as zip_ref:
                        zip_ref.extractall(manifest_dir)
                except zipfile.BadZipFile as e:
                    print(f"ERROR: Could not extract zip file {zip_file_path}."
                          f"\n{str(e)}")
                    if raise_exceptions:
                        raise

                # Remove zip file
                os.remove(zip_file_path)

                zip_manifest_path = r.get("manifestPath")
                if not zip_manifest_path:
                    zip_manifest_path = "mysql-shell-plugins-manifest.json"

                manifest_path = manifest_dir
                for path_item in zip_manifest_path.split('/'):
                    manifest_path = os.path.join(manifest_path, path_item)

                if not os.path.isfile(manifest_path):
                    shutil.rmtree(manifest_dir)
                    print(f"ERROR: Cannot open file '{manifest_path}'.")
                    return

                with open(manifest_path, "r") as manifest_file:
                    manifest = manifest_file.read()

                shutil.rmtree(manifest_dir)
            else:
                manifest = general.download_file(r.get("url"))

            # Load the current plugin manifest of the repo
            if manifest:
                plugins = json.loads(manifest).get("plugins")

                # Add id, installed, installedVersion,
                # installedDevelopmentStage and updateAvailable fields to all
                # plugins
                for p in plugins:
                    p["id"] = i
                    i += 1

                    # Set installed, installedVersion and updateAvailable
                    name = p.get("name")
                    installed = name in dir(mysqlsh.globals)
                    p["installed"] = installed
                    if installed:
                        latest_version = p.get("latestVersion")
                        plugin = getattr(mysqlsh.globals, name)
                        if plugin and "version" in dir(plugin):
                            installed_version = plugin.version()
                            p["installedVersion"] = installed_version

                            if parse_version(latest_version) > \
                                    parse_version(installed_version):
                                p["updateAvailable"] = True

                            version_data = get_plugin_version_data(
                                p, installed_version)
                            if version_data:
                                installed_dev_stage = version_data.get(
                                    "developmentStage")
                                if installed_dev_stage:
                                    p["installedDevelopmentStage"] = \
                                        installed_dev_stage.upper()

                # Filter out installed plugins if needed
                if plugin_filter == Filter.ONLY_INSTALLED:
                    plugins = [p for p in plugins if p.get("installed")]
                elif plugin_filter == Filter.NOT_INSTALLED:
                    plugins = [p for p in plugins if not p.get("installed")]
                elif plugin_filter == Filter.UPDATABLE:
                    plugins = [
                        p for p in plugins if p.get("updateAvailable", False)
                    ]

                # Add list of plugins to the repo
                r["plugins"] = plugins

        return repos

    except json.JSONDecodeError as e:
        print(f"ERROR: Could not parse JSON file. {str(e.msg)}, "
              f"line: {e.lineno}, col: {e.colno}")
        if raise_exceptions:
            raise

    except Exception as e:
        print(f"ERROR: Could not get the list of repositories.\n{str(e)}")
        if raise_exceptions:
            raise


def get_plugin_version_data(plugin, version):
    """Returns the plugin version data for a given version

    Args:
        plugin (dict): A dict representing a plugin
        version (str): The version to look up

    Returns:
        The version data as a dict or None if the version is not found
    """

    versions = plugin.get("versions")
    if not versions:
        return

    for v in versions:
        if v.get("version") == version:
            return v


def get_plugin_count(repositories):
    """Returns the total number of plugin

    Args:
        repositories (dict): A list of repositories with their plugins.

    Returns:
        The number of plugins
    """
    i = 0
    if repositories:
        for r in repositories:
            plugins = r.get("plugins")
            if not plugins:
                continue
            i += len(plugins)

    return i


def get_installed_plugin_count(repositories):
    """Returns the total number of plugin

    Args:
        repositories (dict): A list of repositories with their plugins.

    Returns:
        The number of plugins
    """
    i = 0
    for r in repositories:
        plugins = r.get("plugins")
        if not plugins:
            continue
        for p in plugins:
            if p.get("installed", False):
                i += 1

    return i


def get_available_update_count(repositories):
    """Returns the number of available plugin updates

    Args:
        repositories (dict): A list of repositories with their plugins.
        print_info (bool): If set to True, information about the updates will
            be printed

    Returns:
        A tuple containing the number of available updates and info text
    """
    updateable_count = 0
    for r in repositories:
        plugins = r.get("plugins")
        if not plugins:
            continue
        for p in plugins:
            if p.get("updateAvailable", False):
                updateable_count += 1

    if updateable_count == 1:
        out = (f"One update is available. "
               "Use plugins.update() to install the update.")
    elif updateable_count > 1:
        out = (f"{updateable_count} updates are available. "
               "Use plugins.update() to install the updates.")
    else:
        out = "No updates available."

    return updateable_count, out


def format_plugin_listing(repositories, add_description=True):
    """Returns a formatted list of plugins.

    Args:
        repositories (list): A list of repositories with their plugins.

    Returns:
        The formated list as string
    """
    i = 1

    out = ""
    header = (f"   # {'Name':20} {'Caption':34} {'Version':16} "
              f"{'Installed':16}")
    sep = f"---- {'-'*20} {'-'*34} {'-'*16} {'-'*16}"
    for r in repositories:
        plugins = r.get("plugins")
        if not plugins:
            continue

        repo_name = r.get('name', 'Repository: ?')
        if out == "":
            out += f"{header}\n{sep}\n"
        else:
            if not add_description:
                out += '\n'

        #out += f"{repo_name}\n{'='*94}\n"

        for p in plugins:
            id = p.get("id", i)
            name = p.get("name")
            name = name[:18] + '..' if len(name) > 20 else name

            # Caption
            caption = p.get("caption")
            caption = re.sub(
                r'[\n\r]', ' ',
                caption[:32] + '..' if len(caption) > 34 else caption)
            # Description
            desc = p.get("description")
            desc = re.sub(r'[\n\r]', ' ',
                          desc[:73] + '..' if len(desc) > 75 else desc)

            # Latest Version
            latest_v = p.get("latestVersion")

            # Development Stage
            dev_stage = ""
            version_data = get_plugin_version_data(p, latest_v)
            if version_data:
                dev_stage = version_data.get("developmentStage")
                if dev_stage:
                    dev_stage = dev_stage.upper()

            version_str = latest_v + " " + dev_stage

            # Installed Version
            installed = p.get("installed", False)
            installed_version = p.get("installedVersion", "?.?.?")
            installed_dev_stage = p.get("installedDevelopmentStage", "??")
            update_available = "^" if p.get("updateAvailable", False) else ""

            installed_str = installed_version + " " + installed_dev_stage + \
                update_available

            if installed:
                out += f"*{id:>3} "
            else:
                out += f"{id:>4} "
            out += (f"{name:20} {caption:34} {version_str:16} "
                    f"{installed_str if installed else 'No':16}\n")

            if add_description:
                out += f"     {desc}\n\n"

            i += 1

    return out


def get_plugin(repositories, name=None, interactive=True):
    """Returns a plugin dict

    Args:
        repositories (list): A list of repositories with their plugins.
        name (str): The name of the plugin
        interactive (bool): Whether user input is accepted

    Returns:
        A dict representing the plugin
    """
    selected_item = None
    plugin_count = get_plugin_count(repositories)

    # If there are no plugins, return None
    if plugin_count == 0:
        print("No plugins available.")
        return

    # If no name is given but there is only one plugin, return that one
    # if name is None and plugin_count == 1:
    #     for r in repositories:
    #         plugins = r.get("plugins")
    #         if not plugins:
    #             continue
    #         if len(plugins) > 0:
    #             selected_item = plugins[0]
    if name is None and interactive:
        out = format_plugin_listing(repositories, add_description=False)
        print(out)

        # Let the user choose from the list
        while selected_item is None:
            prompt = mysqlsh.globals.shell.prompt(
                "Please enter the index or name of a plugin: ").strip().lower(
            )
            if not prompt:
                print("Operation cancelled.")
                return

            plugin_count = get_plugin_count(repositories)

            try:
                try:
                    # If the user provided an index, try to map that
                    nr = int(prompt)
                    if nr > 0 and nr <= plugin_count:
                        for r in repositories:
                            plugins = r.get("plugins")
                            if not plugins:
                                continue
                            for p in plugins:
                                if nr == p.get("id"):
                                    selected_item = p
                                    break
                            if selected_item:
                                break
                    else:
                        raise IndexError
                except ValueError:
                    # Search by name
                    for r in repositories:
                        plugins = r.get("plugins")
                        if not plugins:
                            continue
                        for p in plugins:
                            if prompt == p.get("name"):
                                selected_item = p
                                break
                        if selected_item:
                            break

                    if selected_item is None:
                        raise ValueError

            except (ValueError, IndexError):
                print(f"The plugin '{prompt}' was not found. Please try again "
                      "or leave empty to cancel the operation.\n")
                return

        # Add empty line
        print("")
    else:
        for r in repositories:
            plugins = r.get("plugins")
            if not plugins:
                continue
            for p in plugins:
                if name == p.get("name"):
                    selected_item = p
                    break
            if selected_item:
                break
        if not selected_item:
            print(f"The plugin '{name}' was not found.")

    return selected_item


def list_plugins(**kwargs):
    """Lists plugins all available MySQL Shell plugins.

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

    return_formatted = kwargs.get('return_formatted', True)
    interactive = kwargs.get('interactive', True)

    # Get available repositories including the plugin lists
    repositories = get_repositories_with_plugins()
    if not repositories:
        return

    if not return_formatted:
        return repositories
    else:
        # Get number of available updates
        updates_available, update_info = get_available_update_count(
            repositories)

        out = format_plugin_listing(repositories)

        plugin_count = get_plugin_count(repositories=repositories)

        inst_count = get_installed_plugin_count(repositories=repositories)

        if inst_count > 0:
            out += (f"* {inst_count} plugin{'s' if inst_count > 1 else ''} "
                    "installed, ")

        if plugin_count > 0:
            out += (f"{plugin_count} plugin"
                    f"{'s' if plugin_count > 1 else ''}"
                    " total.")

        # If there are available updates, add that information
        if updates_available > 0:
            out += f"\n^ {update_info}"

        if interactive:
            out += (
                "\n\nUse plugins.details() to get more information about a "
                "specific plugin.\n")
        else:
            out += "\n"

        return out


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
    version = kwargs.get("version")
    force_install = kwargs.get("force_install")
    return_object = kwargs.get("return_object", False)
    interactive = kwargs.get("interactive", True)
    printouts = kwargs.get("printouts", True)
    raise_exceptions = kwargs.get("raise_exceptions", False)

    try:
        # Get the list of repositories and their plugins
        repositories = get_repositories_with_plugins(
            plugin_filter=Filter.NONE
            if name or version or force_install else Filter.NOT_INSTALLED,
            printouts=printouts)
        if not repositories:
            return

        # Get the plugin by name or interactive user selection
        plugin = get_plugin(repositories, name, interactive)
        if plugin is None:
            return

        # Get plugin name and caption
        name = plugin.get('name')
        caption = plugin.get('caption')
        module_name = plugin.get('moduleName')

        installed = name in dir(mysqlsh.globals)
        if installed and not force_install:
            print(f"The plugin '{name}' is already installed. Use the "
                  "force_install parameter to re-install it anyway.\n")
            return

        # Get version data
        version = version if version else plugin.get('latestVersion')
        version_data = get_plugin_version_data(plugin=plugin, version=version)
        if not version_data:
            print(
                f"ERROR: Version {version} not found for plugin {caption}.\n")
            return

        shell_version_min = version_data.get('shellVersionMin',None)
        shell_version_max = version_data.get('shellVersionMax',None)

        try:
            validate_shell_version(min=shell_version_min, max=shell_version_max)
        except Exception as e:
            print(str(e))
            return

        if printouts:
            print(f"Installing {caption} ...")

        # Get the right download URL for the plugin
        urls = version_data.get('urls')
        if not urls:
            print(f"ERROR: No URLs available for plugin {caption}.\n")
            return
        # TODO: Check if the shell binary is a community or commercial build
        url = urls.get('community')
        zip_file_name = url.split('/')[-1]
        zip_file_path = general.get_shell_temp_dir(zip_file_name)

        # If the zip file already exists, delete it
        if os.path.isfile(zip_file_path):
            os.remove(zip_file_path)

        # Download the file
        if not general.download_file(url=url, file_path=zip_file_path):
            return

        # Set plugin directory using the module_name of the plugin
        plugin_dir = general.get_plugins_dir(module_name)

        # If the plugin folder already exists, delete it
        if force_install and os.path.isdir(plugin_dir):
            shutil.rmtree(plugin_dir)

        # Extract the zip file
        try:
            with zipfile.ZipFile(zip_file_path, 'r') as zip_ref:
                zip_ref.extractall(plugin_dir)
        except zipfile.BadZipFile as e:
            print(
                f"ERROR: Could not extract zip file {zip_file_path}.\n{str(e)}"
            )
            if raise_exceptions:
                raise

        # Remove zip file
        os.remove(zip_file_path)

        if printouts:
            print(f"{caption} has been installed successfully.\n\n"
                  f"Please restart the shell to load the plugin. To get help "
                  f"type  '\\? {name}' after restart.\n")

        if return_object:
            return plugin
    except Exception as e:
        print(f"ERROR: Could not install the plugin.\n{str(e)}")
        if raise_exceptions:
            raise


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
    interactive = kwargs.get("interactive", True)
    printouts = kwargs.get("printouts", True)
    raise_exceptions = kwargs.get("raise_exceptions", False)

    try:
        # Get the list of repositories and their plugins
        repositories = get_repositories_with_plugins(
            plugin_filter=Filter.ONLY_INSTALLED, printouts=printouts)
        if not repositories:
            return

        # Get the plugin by name or interactive user selection
        plugin = get_plugin(repositories, name, interactive)
        if plugin is None:
            print("Cancelling operation.")
            return

        # Get plugin name and caption
        name = plugin.get('name')
        caption = plugin.get('caption')
        module_name = plugin.get('moduleName')

        if interactive:
            prompt = mysqlsh.globals.shell.prompt(
                f"\nAre you sure you want to uninstall the plugin '{name}'"
                " [YES/no]: ", {
                    'defaultValue': 'yes'
                }).strip().lower()
            if prompt != 'yes':
                print("Operation cancelled.")
                return

        if printouts:
            print(f"Uninstalling {caption} ...")

        # Get plugins_dir
        plugins_dir = general.get_plugins_dir()

        # Set plugin directory using the name of the plugin
        plugin_dir = os.path.join(plugins_dir, module_name)

        # Delete plugin directory and its contents
        shutil.rmtree(plugin_dir)

        if printouts:
            print(f"{caption} has been uninstalled successfully.\n\n"
                  f"Please restart the shell to unload the plugin.\n")
    except Exception as e:
        print(f"ERROR: Could not uninstall the plugin.\n{str(e)}")
        if raise_exceptions:
            raise


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
    interactive = kwargs.get("interactive", True)
    raise_exceptions = kwargs.get("raise_exceptions", False)

    try:
        # Get available repositories including the plugin lists
        repositories = get_repositories_with_plugins(
            plugin_filter=Filter.UPDATABLE)
        if not repositories:
            return

        # Build list of plugins and ask the user to confirm when interactive is
        # set
        plugins = []
        plugin_count = get_plugin_count(repositories=repositories)
        if plugin_count == 0:
            print("No updates available.\n")
            return
        elif plugin_count == 1:
            plugin = None
            if name:
                plugin = get_plugin(repositories=repositories,
                                    name=name,
                                    interactive=False)
                if not plugin:
                    print(f"No update available for plugin '{name}'.")
                    return
            elif plugin_count == 1:
                plugin = get_plugin(repositories=repositories)

            if interactive:
                caption = plugin.get("caption")
                prompt = mysqlsh.globals.shell.prompt(
                    f"Are you sure you want to update the '{caption}' "
                    "[YES/no]: ", {
                        'defaultValue': 'yes'
                    }).strip().lower()
                if prompt != 'yes':
                    print("Operation cancelled.")
                    return

            plugins.append(plugin)
        else:
            for r in repositories:
                plugins = r.get("plugins")
                if not plugins:
                    continue
                plugins.append(plugins)

            print("There are updates pending for the following plugins:")
            for plugin in plugins:
                print(f"  - {plugin.get('caption')}")

            prompt = mysqlsh.globals.shell.prompt(
                f"\nAre you sure you want to update these plugins [YES/no]: ",
                {
                    'defaultValue': 'yes'
                }).strip().lower()
            if prompt != 'yes':
                print("Operation cancelled.")
                return

        # Perform the updates
        for plugin in plugins:
            name = plugin.get("name")
            caption = plugin.get("caption")
            try:
                print(f"Updating {caption} ...")

                install_plugin(name=name,
                               force_install=True,
                               interactive=False,
                               printouts=False,
                               raise_exceptions=True)

                print(f"\nThe update{'s have' if len(plugins) > 1 else ' has'}"
                      " been completed successfully.\n\n"
                      "Please restart the shell to reload the plugin.\n")
            except Exception as e:
                print(f"ERROR: Could not update plugin '{name}'.\n{str(e)}")
                if raise_exceptions:
                    raise

    except Exception as e:
        print(f"ERROR: Could not successfully update all plugins.\n{str(e)}")
        if raise_exceptions:
            raise


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
    interactive = kwargs.get("interactive", True)

    # Get available repositories including the plugin lists
    repositories = get_repositories_with_plugins()

    # Get a plugin to print details about
    plugin = get_plugin(repositories=repositories,
                        name=name,
                        interactive=interactive)
    if not plugin:
        return

    caption = plugin.get('caption', '')
    description = plugin.get('description', '').replace("\n", f"\n{' ' * 13}")
    latest_v = plugin.get('latestVersion', '')
    v_data = get_plugin_version_data(plugin=plugin, version=latest_v)
    latest_v_dev_stage = v_data.get('developmentStage', '').upper()
    shell_ver_min = v_data.get('shellVersionMin','')
    shell_ver_max = v_data.get('shellVersionMax','')
    versions = plugin.get("versions", [])

    print(f"{caption}\n{'-' * len(caption)}\n"
          f"{'Plugin Name:':>20} {plugin.get('name', '')}\n"
          f"{'Latest Version:':>20} {latest_v}\n"
          f"{'Dev Stage:':>20} {latest_v_dev_stage}\n"
          f"{'Min. Shell Version:':>20} {shell_ver_min}\n"
          f"{'Max. Shell Version:':>20} {shell_ver_max}\n"
          f"{'Description:':>20} {description}\n"
          f"{'Available Versions:':>20} {len(versions)}")

    i = 0
    for version in versions:
        v_str = (f"{version.get('version', '?.?.?')} "
                 f"{version.get('developmentStage', '?').upper()}")
        v_str = f"{v_str:>18}"
        out = v_str
        change_list = version.get("changes")
        if change_list and len(change_list) > 0:
            changes = ""
            for change in change_list:
                if changes == "":
                    changes += " - "
                else:
                    changes += f"{' ' * len(v_str)} - "
                changes += change + "\n"
            out += changes
        print(out)
        i += 1

        if interactive and i == 5 and len(versions) > 5:
            rem = len(versions) - i
            prompt = mysqlsh.globals.shell.prompt(
                f"Do you want to print the remaining list of "
                f"{rem} version{'s' if rem > 1 else ''}? [yes/NO]: ", {
                    'defaultValue': 'no'
                }).strip().lower()
            if prompt != 'yes':
                break
