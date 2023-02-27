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

import os.path
import json

import mysqlsh

from mysqlsh.plugin_manager import plugin, plugin_function
from . import general

# Define default repositories
DEFAULT_PLUGIN_REPOSITORIES = [{
    "name":
    "Official MySQL Shell Plugin Repository",
    "description":
    "The official MySQL Shell Plugin Repository maintained by "
    "the MySQL Team at Oracle.",
    "url":
    "https://cdn.mysql.com/mysql_shell/plugins_manifest.zip",
    "manifestPath":
    "manifest/mysql-shell-plugins-manifest.json"
}]


def format_repository_listing(repositories):
    """Returns a formatted list of repositories.

    Args:
        repositories (list): A list of repositories.

    Returns:
        The formated list as string
    """
    out = ""
    i = 1
    for r in repositories:
        out += (f"{i:>4} {r.get('name', '??')}\n"
                f"     {r.get('description', '-')}\n"
                f"     {r.get('url', '??')}\n\n")
        i += 1

    return out


def get_user_repositories(raise_exceptions=False):
    """Fetches the registered user repositories

    Args:
        raise_exceptions (bool): Whether exceptions are raised

    Returns:
        The registered repositories as list
    """
    user_repo_file_path = os.path.join(general.get_shell_user_dir(),
                                       "plugin-repositories.json")
    if os.path.isfile(user_repo_file_path):
        try:
            with open(user_repo_file_path, 'r') as user_repo_file:
                user_repos = json.load(user_repo_file).get("repositories")

            return user_repos
        except json.JSONDecodeError as e:
            print(
                f"ERROR: Could not parse JSON file '{user_repo_file_path}'.\n"
                f"{str(e.msg)}, line: {e.lineno}, col: {e.colno}")
            if raise_exceptions:
                raise
            return
        except OSError as e:
            print(f"ERROR: Error reading file '{user_repo_file_path}'.\n"
                  f"{str(e)}")
            if raise_exceptions:
                raise
    else:
        return []


def set_user_repositories(user_repos, raise_exceptions=False):
    """Stores the registered user repositories

    Args:
        user_repos (list): The list of
        raise_exceptions (bool): Whether exceptions are raised

    Returns:
        True on success
    """
    user_repo_file_content = {
        "fileType": "MySQL Shell Plugin Repositories",
        "version": "0.0.1",
        "repositories": user_repos
    }

    user_repo_file_path = os.path.join(general.get_shell_user_dir(),
                                       "plugin-repositories.json")

    try:
        with open(user_repo_file_path, 'w') as user_repo_file:
            json.dump(user_repo_file_content, user_repo_file, indent=4)

        return True
    except OSError as e:
        print(f"ERROR: Error writing to file '{user_repo_file_path}'.\n"
              f"{str(e)}")
        if raise_exceptions:
            raise
        return False


@plugin_function('plugins.repositories.list')
def get_plugin_repositories(**kwargs):
    """Lists all registered plugin repositories

    This function will list all registered MySQL Shell plugin repositories.
    To add a repository use the plugins.repositories.add() function.

    Args:
        **kwargs: Optional parameters

    Keyword Args:
        return_formatted (bool): If set to true, a list object is returned.
        raise_exceptions (bool): Whether exceptions are raised

    Returns:
        None or a list of dicts representing the plugins
    """

    return_formatted = kwargs.get('return_formatted', True)
    raise_exceptions = kwargs.get('raise_exceptions', False)

    # Init the repo list with the default repositories
    repos = DEFAULT_PLUGIN_REPOSITORIES.copy()

    user_repos = get_user_repositories(raise_exceptions=raise_exceptions)
    if user_repos:
        repos.extend(user_repos)

    if not return_formatted:
        return repos
    else:
        # cSpell:ignore repositor
        print("Registered MySQL Shell Plugin Repositories.\n\n"
              f"{format_repository_listing(repositories=repos)}"
              f"Total of {len(repos)} repositor"
              f"{'y' if len(repos) == 1 else 'ies'}.\n")


@plugin_function('plugins.repositories.add')
def add_plugin_repository(url=None, **kwargs):
    """Adds a new plugin repository

    By calling this function a new plugin repository can be added. The url
    parameter allows for shortcuts.

    'domain.com' expands to https://domain.com/mysql-shell-plugins-manifest.json
    'domain.com/plugins' expands to https://domain.com/plugins/mysql-shell-plugins-manifest.json
    'github/username' expands to https://raw.githubusercontent.com/username/mysql-shell-plugins/master/mysql-shell-plugins-manifest.json
    'github/username/repo' expands to https://raw.githubusercontent.com/username/repo/master/mysql-shell-plugins-manifest.json

    Args:
        url (str): The url of a MySQL Shell plugins repository.
        **kwargs: Optional parameters

    Keyword Args:
        interactive (bool): Whether user input is accepted
        raise_exceptions (bool): Whether exceptions are raised

    Returns:
        None
    """
    interactive = kwargs.get('interactive', True)
    raise_exceptions = kwargs.get('raise_exceptions', True)

    if not url and interactive:
        print("To add a new MySQL Shell plugin repository the URL of the "
              "repository manifest file\nneeds to be specified.\n\n"
              "Examples:\n"
              "    domain.com\n"
              "    domain.com/plugins\n"
              "    github/username\n"
              "    github/username/repository\n")
        url = mysqlsh.globals.shell.prompt(
            f"Please enter the URL of the plugin repository: ", {
                'defaultValue': ''
            }).strip()

    if not url:
        print("No URL given. Cancelling Operation")
        return

    if url.lower().startswith("github/"):
        github_str = url.split("/")
        if len(github_str) == 1:
            print("A github user needs to be specified")
            return
        github_user = github_str[1]

        # Get the github repo
        if len(github_str) == 2:
            github_repo = "mysql-shell-plugins"
        else:
            github_repo = github_str[2]

        url = (
            f"https://raw.githubusercontent.com/{github_user}/{github_repo}/"
            f"master/mysql-shell-plugins-manifest.json")
    else:
        # Add https:// if not specified
        if not url.startswith("https://") and not url.startswith("http://"):
            url = f"https://{url}"

        if not url.endswith(".json"):
            if url.endswith("/"):
                url = f"{url}mysql-shell-plugins-manifest.json"
            else:
                url = f"{url}/mysql-shell-plugins-manifest.json"

    try:
        manifest = general.download_file(url)
        if not manifest:
            return

        manifest_content = json.loads(manifest)

        repository = manifest_content.get("repository")

        name = repository.get('name')
        if not name:
            print("The given repository misses the 'name' property.")
            return
        description = repository.get('description')
        if not name:
            print("The given repository misses the 'description' property.")
            return

        plugins = manifest_content.get('plugins')
        if not plugins or len(plugins) == 0:
            print("The repository does not contain any plugins. "
                  "Operation cancelled.")
            return

        # Warn the user about possible consequences
        print("\nWARNING:\n"
              "You are about to add an external MySQL Shell plugin repository."
              "\nExternal plugin repositories and their plugins complement\n"
              "the functionality of MySQL Shell and can contain system\n"
              "level software that could be potentially harmful to your\n"
              "system. Please review the description below and only proceed\n"
              "if you have obtained the external plugin repository URL from\n"
              "a trusted source.\n\n"
              "Oracle and its affiliates cannot be held responsible for\n"
              "any potential harm caused by using plugins from external "
              "sources.\n")

        # Ensure the user will explicitly confirm the full URL
        print(f"{'Repository :'} {name}\n"
              f"{'Description:'} {description}\n"
              f"URL: {url}\n\n"
              "The repository contains the following plugins:\n")
        for plugin in plugins:
            print(f"  - {plugin.get('caption')}\n")

        prompt = mysqlsh.globals.shell.prompt(
            f"Are you sure you want to add the repository '{name}'"
            " [yes/NO]: ", {
                'defaultValue': 'no'
            }).strip().lower()
        if prompt != 'yes':
            print("Operation cancelled.")
            return

        print("Fetching current user repositories...")

        user_repos = get_user_repositories(raise_exceptions=raise_exceptions)
        if user_repos is None:
            return

        # Check if the given repository is already in the list
        for r in user_repos:
            if r.get("url").lower() == url.lower():
                print(f"The repository '{name}' has already been added.\n")
                return

        print(f"Adding repository '{name}'...")

        user_repos.append({
            "name": name,
            "description": description,
            "url": url
        })

        if set_user_repositories(user_repos):
            print(f"Repository '{name}' successfully added.\n")

    except json.JSONDecodeError as e:
        print(f"ERROR: Could not parse JSON file. {str(e.msg)}, "
              f"line: {e.lineno}, col: {e.colno}")
        if raise_exceptions:
            raise
        return


def search_user_repo(url, user_repos):
    # Search by name
    nr = 0
    found = False
    for r in user_repos:
        if r.get("url") == url:
            found = True
            break
        nr += 1

    if found:
        return nr
    else:
        return None


@plugin_function('plugins.repositories.remove')
def remove_plugin_repository(**kwargs):
    """Removes a registered plugin repository

    This function will remove a shell plugin repository previously registered.

    Args:
        **kwargs: Optional parameters

    Keyword Args:
        url (str): The url of a MySQL Shell plugins repository.
        interactive (bool): Whether user input is accepted
        raise_exceptions (bool): Whether exceptions are raised

    Returns:
        None
    """
    url = kwargs.get('url')
    interactive = kwargs.get('interactive', True)
    raise_exceptions = kwargs.get('raise_exceptions', False)

    user_repos = get_user_repositories(raise_exceptions=raise_exceptions)
    if not user_repos:
        print("No custom MySQL Shell plugin repositories registered.\n")
        return

    repo_index = None
    if url:
        repo_index = search_user_repo(url, user_repos)
        if repo_index is None:
            print(f"No user repository matches '{url}'")

    if repo_index is None and interactive:
        print("Removing a plugin repository.\n\n"
              f"{format_repository_listing(user_repos)}")

        # Let the user choose from the list
        while repo_index is None:
            prompt = mysqlsh.globals.shell.prompt(
                "Please enter the index or URL of a repository: ").strip()

            if prompt:
                try:
                    try:
                        # If the user provided an index, try to map that
                        nr = int(prompt)
                        if nr > 0 and nr <= len(user_repos):
                            repo_index = nr - 1
                        else:
                            raise IndexError
                    except ValueError:
                        repo_index = search_user_repo(prompt, user_repos)

                        if repo_index is None:
                            raise ValueError

                except (ValueError, IndexError):
                    print(f"The repository '{prompt}' was not found. Please try "
                          "again or leave empty to cancel the operation.\n")
            else:
                break

    if repo_index is None:
        print("No valid URL provided. Cancelling operation.")
    else:
        url = user_repos[repo_index].get("url", '')
        print(f"\nRemoving repository '{url}'...")
        del user_repos[repo_index]

        if set_user_repositories(user_repos):
            print(f"Repository successfully removed.\n")
