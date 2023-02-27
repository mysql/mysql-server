# Copyright (c) 2020, Oracle and/or its affiliates.
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

import certifi
import platform
import os
import urllib.error
import urllib.request
import stat
import ssl

from pathlib import Path

# Define plugin version
VERSION = "0.0.1"

URL_OPEN_TIMEOUT_SECONDS = 10


def get_shell_user_dir(*argv):
    """Returns the MySQL Shell's user directory

    The directory will be created if it does not exist yet

    Args:
        *argv: The list of directories to be added to the path

    Returns:
        The plugin directory path as string
    """
    if "MYSQLSH_USER_CONFIG_HOME" in os.environ:
        shell_dir = os.environ["MYSQLSH_USER_CONFIG_HOME"]
    else:
        home_dir = Path.home()
        if not home_dir:
            raise Exception("No home directory set")
        os_name = platform.system()
        if os_name == "Windows":
            shell_dir = os.path.join(
                home_dir, "AppData", "Roaming", "MySQL", "mysqlsh"
            )
        else:
            shell_dir = os.path.join(home_dir, ".mysqlsh")

    # Ensure the path exists
    try:
        Path(shell_dir).mkdir(parents=True)
    except FileExistsError:
        pass

    for arg in argv:
        shell_dir = os.path.join(shell_dir, arg)

    return shell_dir


def get_shell_temp_dir(*argv):
    """Returns the MySQL Shell's temp directory

    The directory will be created if it does not exist yet

    Args:
        *argv: The list of directories to be added to the path

    Returns:
        The temp directory path as string
    """
    temp_dir = get_shell_user_dir("temp")

    # Ensure the path exists
    try:
        Path(temp_dir).mkdir(parents=True)
    except FileExistsError:
        pass

    for arg in argv:
        temp_dir = os.path.join(temp_dir, arg)

    return temp_dir


def get_plugins_dir(*argv):
    """Returns the MySQL Shell's plugin directory

    The directory will be created if it does not exist yet

    Args:
        *argv: The list of directories to be added to the path

    Returns:
        The plugin directory path as string
    """
    from pathlib import Path
    import os.path

    plugins_dir = get_shell_user_dir("plugins")

    # Ensure the path exists
    try:
        Path(plugins_dir).mkdir(parents=True)
    except FileExistsError:
        pass

    for arg in argv:
        plugins_dir = os.path.join(plugins_dir, arg)

    return plugins_dir


def download_file(url, file_path=None, raise_exceptions=False, ctx=None):
    """Downloads a file from a given URL

    Args:
        url (str): The URL to the text file to download
        file_path (str): The file to store the downloaded file
        raise_exceptions (bool):  Whether exceptions are raised

    Returns:
        The contents of the file decoded to utf-8 if no file is given or True
        on success or None on failure
    """
    try:
        os_name = platform.system()
        if ctx is None and os_name == "Darwin":
            # NOTE: On this call we require raising exceptions so if it fails in OSX
            # It still goes and attempt using the certifi-ca below.
            return download_file(url,
                                 file_path=file_path,
                                 raise_exceptions=True,
                                 ctx=ssl.create_default_context(cafile="/private/etc/ssl/cert.pem"))

        with urllib.request.urlopen(url, timeout=URL_OPEN_TIMEOUT_SECONDS, context=ctx) as response:
            data = response.read()

        if not file_path:
            return data.decode("utf-8")
        else:
            with open(file_path, "wb") as out_file:
                out_file.write(data)
            return True

    except urllib.error.HTTPError as e:
        if raise_exceptions:
            raise
        if e.code == 404:
            print(f"Could not download file from {url}\nERROR: {str(e)}")
            return
        else:
            raise
    except urllib.error.URLError as e:
        # Since the reason might be either text or another exception, we get the
        # string representation so we can validate the error below.
        reason = str(e.reason)
        if 'CERTIFICATE_VERIFY_FAILED' in reason and ctx is None:
            cafile = install_ssl_certificates()
            return download_file(url,
                                 file_path=file_path,
                                 raise_exceptions=raise_exceptions,
                                 ctx=ssl.create_default_context(cafile=cafile))
        else:
            print(f"Could not download file from {url}\nERROR: {str(e)}")
            if raise_exceptions:
                raise

    return None


def install_ssl_certificates():
    STAT_0o775 = (
        stat.S_IRUSR
        | stat.S_IWUSR
        | stat.S_IXUSR
        | stat.S_IRGRP
        | stat.S_IWGRP
        | stat.S_IXGRP
        | stat.S_IROTH
        | stat.S_IXOTH
    )

    # Shell CA File Path
    shell_cafile = os.path.join(get_shell_user_dir(), "certifi-ca.pem")

    print("Installing SSL certificate...")
    relpath_to_certifi_cafile = os.path.relpath(certifi.where())
    print(" -- removing any existing file or link")
    try:
        os.remove(shell_cafile)
    except FileNotFoundError:
        pass
    print(" -- creating symlink to certifi certificate bundle")
    os.symlink(relpath_to_certifi_cafile, shell_cafile)
    print(" -- setting permissions")
    os.chmod(shell_cafile, STAT_0o775)
    print(" -- update complete")

    return shell_cafile
