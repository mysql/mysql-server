# Copyright (c) 2012, 2019 Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

"""Provides specialization of ABClusterHost for remote hosts using Paramiko."""

import errno
import stat
import time
import ntpath
import logging
import os.path
import socket
import contextlib
import posixpath
import threading
import paramiko
import util
from util import bcolors
import clusterhost
from clusterhost import ABClusterHost

CONN_LOCK = threading.Lock()
RUNTIME = {} # Dictionary which will never change top-level,
RUNTIME["CON_AR"] = [] # so no need to mark this as global.

_logger = logging.getLogger(__name__)

def quote_if_contains_space(s):
    """-"""
    if ' ' in s:
        return '"'+s+'"'
    return s

class RemoteExecException(clusterhost.ExecException):
    """Exception type thrown whenever os-command execution fails on
    a remote host. """
    def __init__(self, hostname, cmd, exitstatus, out):
        self.hostname = hostname
        self.cmd = cmd
        self.exitstatus = exitstatus
        self.out = out.read()
    def __str__(self):
        return 'Command `{self.cmd}\', running on {self.hostname} exited with \
        {self.exitstatus}:{self.out}'.format(self=self)

def cleanup_connections():
    """Purge array of established SSH connections."""
    global CONN_LOCK
    _logger.warning(bcolors.WARNING + "RCH--> Cleaning up connArray." + bcolors.ENDC)
    with CONN_LOCK:
        if len(RUNTIME["CON_AR"]) >= 1:
            try:
                for x in range(len(RUNTIME["CON_AR"])):
                    if RUNTIME["CON_AR"][x] is not None:
                        RUNTIME["CON_AR"][x].close()
            except (paramiko.SSHException, IndexError): # Nothing. It is closed.
                _logger.warning(bcolors.WARNING + "RCH--> SSHExc or IndexError" + bcolors.ENDC)
        del RUNTIME["CON_AR"][:]

def delete_connection(host):
    """Remove SSH connection from array of active connections."""
    global CONN_LOCK
    _logger.warning(bcolors.WARNING + "\nRCH--> Deleting remote conn. to %s." + bcolors.ENDC, host)
    with CONN_LOCK:
        _logger.debug("\nRCH--> Deleting remote conn. to %s, Len ConnArr is %s.", host,
                      len(RUNTIME["CON_AR"]))
        if len(RUNTIME["CON_AR"]) >= 1:
            try:
                RUNTIME["CON_AR"][:] = [x for x in RUNTIME["CON_AR"] if not x.host == host]
                RUNTIME["CON_AR"] = [x for x in RUNTIME["CON_AR"] if x != '']
            except IndexError: # Nothing. It is closed.
                _logger.warning(bcolors.WARNING + "RCH--> IndexError" + bcolors.ENDC)
        wrn_msg = "\nRCH--> Deleting remote conn. to %s, NEW Len ConnArr is %s."
        _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, host, len(RUNTIME["CON_AR"]))

def check_connected(sshcl):
    """Check if SSH connection is newly/still connected."""
    if sshcl is not None:
        _logger.debug("RCH--> Is %s connected?", sshcl.host)
        try:
            transport = sshcl.get_transport()
            if transport is not None:
                transport.send_ignore()
                _logger.debug("RCH--> %s is connected", sshcl.host)
                return True
            return False
        except Exception:
            _logger.debug("RCH--> NOT connected")
            return False
    else:
        _logger.debug("RCH--> sshcl1 was None")
        return False

def list_remote_hosts():
    """Provide FE with list of hosts we're permanently connected to."""
    _logger.debug("RCH--> Listing hosts in connArray.")
    host_arr = []
    if len(RUNTIME["CON_AR"]) < 1:
        return host_arr #Do not wait.
    try:
        for x in range(len(RUNTIME["CON_AR"])):
            if RUNTIME["CON_AR"][x] is not None:
                host_arr.append(RUNTIME["CON_AR"][x].host)
    except IndexError:
        _logger.warning(bcolors.WARNING + "RCH--> IndexError" + bcolors.ENDC)
    # Let the calling code sort out what to do next depending on configuration.
    return host_arr

def add_permconn(host):
    """Add new permanent SSH connection to remote host to array."""

    global CONN_LOCK
    with CONN_LOCK:
        RUNTIME["CON_AR"].append(paramiko.SSHClient())
        x = len(RUNTIME["CON_AR"])-1
        _logger.debug('\nRCH--> Adding perm. conn. %s for Host=%s', x, host)
        RUNTIME["CON_AR"][x].host = host
        RUNTIME["CON_AR"][x].system = None
        RUNTIME["CON_AR"][x].processor = None
        RUNTIME["CON_AR"][x].osver = None
        RUNTIME["CON_AR"][x].osflavor = None
        RUNTIME["CON_AR"][x].load_system_host_keys()
        RUNTIME["CON_AR"][x].set_missing_host_key_policy(paramiko.AutoAddPolicy())
        _logger.debug('\nRCH--> Opening new permanent conn. %s to host %s.', x,
                      RUNTIME["CON_AR"][x].host)
        #Not really necessary to return index as we use Host member to synchronize.
        return x

def connect_permconn(host, user, pwd, pkey, keybased, index):
    """Connect permanent SSH connection to remote host."""
    # JSON decoder can not handle below errors since they are tuples, not K/W strings. I.e.
    # TypeError: key ('129.146.115.208', 22) is not a string. Must find batter way to handle,
    # either here or in request_handler #1761.
    # The error we're most concerned with here is:
    # NoValidConnectionsError: [Errno None] Unable to connect to port 22 on 129.146.115.208
    # First see if there is SSH service to connect socket with.
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    result = sock.connect_ex((host, 22))
    # we need socket we can manipulate in SSHClient object, this is just a test so close.
    sock.close()
    if result > 0:
        _logger.error(bcolors.FAIL + "\nRCH--> No SSH service at %s." + bcolors.ENDC, host)
        return False

    if keybased is False:
        try:
            _logger.debug('RCH--> ' + "PERMConnEnter NO KEY, NO PASSWORD")
            RUNTIME["CON_AR"][index].connect(hostname=host, username=user, password=pwd, pkey=None,
                                             look_for_keys=False, banner_timeout=4, timeout=5)
            return True
        except paramiko.ssh_exception.BadAuthenticationType as e:
            _logger.error(bcolors.FAIL + "\nRCH--> Unsupported authentication type." + bcolors.ENDC)
        except paramiko.ssh_exception.AuthenticationException as e:
            _logger.error(bcolors.FAIL + "\nRCH--> Bad credentials provided" + bcolors.ENDC)
        except paramiko.ssh_exception.NoValidConnectionsError:
            _logger.error(bcolors.FAIL + "\nRCH--> No valid connections..." +bcolors.ENDC)
        except paramiko.ssh_exception.SSHException as e: # any other exception
            _logger.error(bcolors.FAIL + "\nRCH--> %s" + bcolors.ENDC, e.message)
        except socket.error:
            _logger.error(bcolors.FAIL + "\nRCH--> Socket error." +bcolors.ENDC)
    else:
        # We have key file pointed out for us.
        privatekeyfile = pkey or  os.path.expanduser('~/.ssh/id_rsa')
        # Is it valid file?
        if not os.path.isfile(privatekeyfile):
            _logger.error(bcolors.FAIL + "RCH--> %s doesn't exist!" + bcolors.ENDC, privatekeyfile)
            return False
        try:
            mykey = paramiko.RSAKey.from_private_key_file(privatekeyfile, password=pwd)
        except paramiko.ssh_exception.SSHException as e:
            _logger.error(bcolors.FAIL + "\nRCH--> %s" + bcolors.ENDC, e.message)
            return False

        try:
            RUNTIME["CON_AR"][index].connect(hostname=host, username=user, password=pwd, pkey=mykey,
                                             look_for_keys=False, banner_timeout=4, timeout=5)
            return True
        except paramiko.ssh_exception.BadAuthenticationType as e:
            _logger.error(bcolors.FAIL + "\nRCH--> Unsupported authentication type." + bcolors.ENDC)
        except paramiko.ssh_exception.AuthenticationException as e:
            _logger.error(bcolors.FAIL + "\nRCH--> Bad credentials provided" + bcolors.ENDC)
        except paramiko.ssh_exception.NoValidConnectionsError:
            _logger.error(bcolors.FAIL + "\nRCH--> No valid connections..." +bcolors.ENDC)
        except paramiko.ssh_exception.SSHException as e: # any other exception
            _logger.error(bcolors.FAIL + "\nRCH--> %s" + bcolors.ENDC, e.message)
        except socket.error:
            _logger.error(bcolors.FAIL + "\nRCH--> Socket error." +bcolors.ENDC)

    return False
class RemoteClusterHost(ABClusterHost):
    """Implements the ABClusterHost interface for remote hosts. Wraps a paramiko.SSHClient and uses
    this to perform tasks on the remote host."""

    def __init__(self, host, key_based=None, username=None, password=None, key_file=None):
        super(type(self), self).__init__()
        self.host = host
        _logger.debug('RCH--> ' + "Host="+host)
        self.user = username
        # if password is not None:
        #     self.pwd = password
        # else:
        #     self.pwd = None
        self.pwd = password
        _logger.debug('RCH--> ' + "Set self.pwd")
        # Create new or reuse existing connection:
        ConIdx = -1
        if RUNTIME["CON_AR"]:
            for x in range(len(RUNTIME["CON_AR"])):
                if hasattr(RUNTIME["CON_AR"][x], "host"):
                    if RUNTIME["CON_AR"][x].host == host:
                        _logger.debug('RCH--> Reusing PERMConn[%s].', x)
                        ConIdx = x
                        if check_connected(RUNTIME["CON_AR"][ConIdx]):
                            try:
                                self.__client = RUNTIME["CON_AR"][ConIdx]
                                self.__sftp = RUNTIME["CON_AR"][ConIdx].open_sftp()
                                _logger.debug('RCH--> PERMConn[%s] is alive.', ConIdx)
                            except Exception:
                                # We definitely have failed connect attempt.
                                _logger.warning(bcolors.WARNING + "\nRCH--> PERMConn[%s] is "
                                                "NOT alive." + bcolors.ENDC, ConIdx)
                                # Then try getting new one.
                                delete_connection(self.host)
                                ConIdx = add_permconn(self.host)
                                if connect_permconn(self.host, self.user, self.pwd, key_file,
                                                    key_based, ConIdx):
                                    if check_connected(RUNTIME["CON_AR"][ConIdx]):
                                        self.__client = RUNTIME["CON_AR"][ConIdx]
                                        self.__sftp = RUNTIME["CON_AR"][ConIdx].open_sftp()
                                    else:
                                        # delete dead connection as we can not confirm it's alive
                                        delete_connection(self.host)
                                else:
                                    # delete dead connection as there was error connecting
                                    delete_connection(self.host)
                        else:
                            _logger.debug("\nRCH--> PERMConn[%s] is not alive. Recreating.", ConIdx)
                            delete_connection(self.host)
                            ConIdx = add_permconn(self.host)
                            _logger.debug("RCH--> New PERMConn[%s] created.", ConIdx)
                            if connect_permconn(self.host, self.user, self.pwd, key_file,
                                                key_based, ConIdx):
                                if check_connected(RUNTIME["CON_AR"][ConIdx]):
                                    self.__client = RUNTIME["CON_AR"][ConIdx]
                                    self.__sftp = RUNTIME["CON_AR"][ConIdx].open_sftp()
                                else:
                                    # delete dead connection as we can not confirm it's alive
                                    delete_connection(self.host)
                            else:
                                # delete dead connection as there was error connecting
                                delete_connection(self.host)
                        break

        if not RUNTIME["CON_AR"] or ConIdx <= -1:
            #ConnArr is empty OR no connection found for host. Add new in protected code.
            ConIdx = add_permconn(self.host)
            if connect_permconn(self.host, self.user, self.pwd, key_file,
                                key_based, ConIdx):
                if check_connected(RUNTIME["CON_AR"][ConIdx]):
                    self.__client = RUNTIME["CON_AR"][ConIdx]
                    self.__sftp = RUNTIME["CON_AR"][ConIdx].open_sftp()
                    _logger.debug('\nRCH--> ' + "PERMConn[%s] is alive (new conn).", ConIdx)
                else:
                    # delete dead connection as we can not confirm it's alive
                    _logger.warning(bcolors.WARNING + "\nRCH--> PERMConn[%s] is NOT alive "
                                    "(new conn)." + bcolors.ENDC, ConIdx)
                    delete_connection(self.host)
                    raise Exception("No valid connection!")
            else:
                # delete dead connection as there was error connecting
                _logger.warning(bcolors.WARNING + "\nRCH--> PERMConn[%s] is NOT alive (new conn)."
                                + bcolors.ENDC, ConIdx)
                delete_connection(self.host)
                raise Exception("No valid connection!")

        self.system = None
        self.processor = None
        self.osver = None
        self.osflavor = None

    def close(self):
        """-"""
        self.drop()

    @property
    def client(self):
        """Returning Paramiko.SSHClient()"""
        return self.__client

    @property
    def sftp(self):
        """Returning Paramiko.SSHClient().sftp connection"""
        return self.__sftp

    def _get_system_tuple(self):
        _logger.debug('RCH--> remote_ch, system tuple')
        if RUNTIME["CON_AR"]:
            for x in range(len(RUNTIME["CON_AR"])):
                a = hasattr(RUNTIME["CON_AR"][x], "host") and RUNTIME["CON_AR"][x].host == self.host
                hlp_str = hasattr(RUNTIME["CON_AR"][x], "system") and RUNTIME["CON_AR"][x].system
                if a and hlp_str:
                    _logger.debug("RCH--> Reusing system tuple.")
                    return (RUNTIME["CON_AR"][x].system, RUNTIME["CON_AR"][x].processor, \
                        RUNTIME["CON_AR"][x].osver, RUNTIME["CON_AR"][x].osflavor)

        preamble = None
        system = None
        processor = None
        osver = None
        osflavor = None
        hlp_str = None
        try:
            preamble = self.exec_blocking(['uname', '-sp']) #9009 on Win10
        except RemoteExecException:
            _logger.warning(bcolors.WARNING + '\nRCH--> executing uname failed - assuming '
                            'Windows.' + bcolors.ENDC)

            system = "Windows"
            res = self.exec_blocking(['cmd.exe', '/c', 'wmic', 'os', 'get',
                                      'Caption,OSArchitecture,Version', '/value']).split('\r')
            for t in res:
                if t.strip() != "":
                    if t.find("Caption") != -1:
                        osflavor = t.split("Caption=")[1]
                    elif t.find("OSArchitecture") != -1:
                        processor = t.split("OSArchitecture=")[1]
                        if processor.find("64") != -1:
                            processor = "AMD64"
                    elif t.find("Version") != -1:
                        osver = t.split("Version=")[1].split(".")[0]
        else: # no error executing uname -sp
            uname = preamble
            _logger.debug('RCH--> uname=%s', uname)
            (system, processor) = uname.split(' ')
            # When processing output of CAT looking for specific key, this is not relevant.(Linux)
            if 'CYGWIN' in system:
                system = 'CYGWIN'
            elif 'Darwin' in system:
                system = "Darwin"
                osver = str(self.exec_blocking(['uname', '-r']))
                osflavor = "MacOSX"
            elif "SunOS" in system:
                system = "SunOS"
                osver = str(self.exec_blocking(['uname', '-v']))
                osflavor = "Solaris"
            elif "Linux" in system:
                system = "Linux"
                # I assume all Linux flavors will have /etc/os-release file.
                if self.file_exists('/etc/os-release'):
                    hlp_str = self.exec_blocking(['cat', '/etc/os-release'])
                    matched_lines = [line for line in hlp_str.split('\n') if "ID=" in line]
                    hlp = (str(matched_lines[0]).split("ID=", 1)[1]).strip('"')
                    osflavor = hlp
                    matched_lines = [line for line in hlp_str.split('\n') if "VERSION_ID=" in line]
                    hlp = (str(matched_lines[0]).split("VERSION_ID=", 1)[1]).strip('"')
                    osver = hlp
                else:
                    #Bail out, no file
                    _logger.warning(bcolors.WARNING + '\nRCH--> OS version (Linux) does not '
                                    'have /etc/os-release file!' + bcolors.ENDC)

        _logger.debug('\nRCH--> remote_ch, system tuple, returning')
        if RUNTIME["CON_AR"]:
            for x in range(len(RUNTIME["CON_AR"])):
                if hasattr(RUNTIME["CON_AR"][x], "host") and RUNTIME["CON_AR"][x].host == self.host:
                    RUNTIME["CON_AR"][x].system = system
                    RUNTIME["CON_AR"][x].processor = processor.strip()
                    RUNTIME["CON_AR"][x].osver = osver
                    RUNTIME["CON_AR"][x].osflavor = osflavor
        return (system, processor.strip(), osver, osflavor)

    def _exec_pkg_cmdv(self, cmdv):
        """For remote hosts the binary is fist copied over using sftp."""
        _logger.debug('RCH--> remote_ch, exec pkg')
        hi = os.path.basename(cmdv[0])
        self.sftp.put(cmdv[0], hi)
        self.sftp.chmod(hi, stat.S_IRWXU)
        return self.exec_cmdv([self.path_module.join('.', hi)] + cmdv[1:-1])

    def _execfast(self, cmdv):
        return self._exec_cmdv(cmdv, None, None)

    def _sftpify(self, path):
        """Since sftp treats all path names as relative to its root we must convert absolute paths
        before using them with sftp. As quick-fix we assume that the sftp root is equal to the drive
        letter of all absolute paths used. If it isn't the sftp operations will fail."""
        # self.path_module.splitdrive(path)[1] essentially does NOTHING on *NIX and messes up Win.
        return path

    def open(self, filename, mode='r'):
        """Forward to paramiko.SFTPClient.open for remote hosts. Wrap in contextlib.closing so that
        clients can use with-statements on it."""
        _logger.debug('RCH--> remote_ch, open')
        return contextlib.closing(self.sftp.open(self._sftpify(filename), mode))

    def drop(self, paths=[]):
        """Close open connections and remove files.
        paths - list of files to remove from host before closing connection
        """
        _logger.debug('RCH--> remote_ch, drop')
        map(self.rm_r1, paths)

    def get(self, filename, localname):
        """Get a file on ABClusterHost and save to ~/.mcc."""
        #File names are complete with path.
        _logger.warning(bcolors.WARNING + "RCH--> sftp-ing log file" + bcolors.ENDC)
        self.sftp.get(filename, localname)

    def put(self, localname, filename):
        """Put file on ABClusterHost."""
        #File names are complete with path.
        _logger.warning(bcolors.WARNING + "RCH--> sftp-ing file to remote host" + bcolors.ENDC)
        self.sftp.put(localname, filename)

    def file_exists(self, path):
        """Test for the existence of a file on the remote host. If the file actually exists, its
        stat object is returned, otherwise None.
        path - file to check the existence of
        """
        _logger.debug('\nRCH--> remote_ch, file exists, %s', path)
        try:
            return self.sftp.stat(self._sftpify(path))
        except IOError as ioerr:
            _logger.warning(bcolors.WARNING + '\nRCH--> remote_ch file_exists, exception, %s' +
                            bcolors.ENDC, path)
            if ioerr.errno == errno.ENOENT:
                log_what = (bcolors.WARNING + '\nRCH--> remote_ch file_exists, exception, %s, '
                            'returning NONE.' + bcolors.ENDC)
                _logger.warning(log_what, path)
                return None
            _logger.warning(bcolors.WARNING + '\nRCH--> stat failure on %s' + bcolors.ENDC, path)
            raise

    def list_dir(self, path):
        """List the files in a directory on the remote host. Forwards to SFTPClient.listdir(), but
        also warns about empty results that may be caused by paramiko not reporting missing execute
        permission on the directory correctly.
        path - directory to list
        """
        _logger.debug('\nRCH--> ' + 'remote_ch,  list dir, %s', path)
        content = self.sftp.listdir(self._sftpify(path))
        if not content:
            m = stat.S_IMODE(self.sftp.stat(path).st_mode)
            for role in ['USR', 'GRP', 'OTH']:
                mask = util.get_fmask('R', role)|util.get_fmask('X', role)
                if (m & mask) != mask:
                    log_what = ('\nRCH--> Directory %s does not have both read and execute '
                                'permission for %s.\nIf you depend on %s for access, the empty '
                                'directory listing may not be correct')
                    _logger.debug(log_what, path, role, role)

        return content

    def mkdir_p(self, path):
        """Provides mkdir -p type functionality on the remote host. That is, all missing parent
        directories are also created. If the directory we are trying to create already exists, we
        do nothing. If path or any of its parents is not a directory an exception is raised.
        path - directory to create on remote host
        """
        _logger.debug('\nRCH--> remote_ch, mkdir_p(%s)', path)
        path = self._sftpify(path)
        pa = self.file_exists(path)
        if pa != None:
            if not util.is_dir(pa):
                raise Exception(self.host+':'+path+' is not a directory')
            return
        # Need to user normpath here since dirname of a directory with a trailing slash
        # is the directory without a slash (a dirname bug?)
        sd = ntpath.splitdrive(path)
        _logger.debug('\nRCH--> sd=%s', sd)
        if sd[1] == '':
            _logger.warning(bcolors.WARNING + '\nRCH--> path=%s is a drive letter. Returning...' +
                            bcolors.ENDC, path)
            return

        np = self.path_module.normpath(path)
        parent = self.path_module.dirname(np)
        assert parent != path
        _logger.debug('\nRCH--> remote_ch, making dir %s', path)
        self.mkdir_p(parent)
        self.sftp.mkdir(np)

    def rm_r1(self, path):
        """Provides rm -r type functionality on the remote host. That is, all
        files and directories are removed recursively.
        path - file or directory to remove
        needed for self.drop()...
        """
        path = self._sftpify(path)
        if util.is_dir(self.sftp.stat(path)):
            for f in self.sftp.listdir(path):
                self.rm_r1(posixpath.join(path, f))
            self.sftp.rmdir(path)
        else:
            self.sftp.remove(path)

    def rm_r(self, path):
        """Provides rm -r type functionality on the remote host. That is, all
        files and directories are removed recursively.
        path - file or directory to remove
        """
        _logger.debug('\nRCH--> remote_ch, deleting %s', path)
        try:
            self.exec_blocking(['uname', '-sp']) #9009 on Win10
        except RemoteExecException:
            # return self._exec_cmdln("rmdir /s " + str(path), {'waitForCompletion': True,
            #   'daemonWait': 2}, None)
            # unfortunately, some Windows versions miss DEL, some RMTREE and some RMDIR, on
            # some not in path etc...
            return self.rm_r1(path)
        else:
            return self._exec_cmdln("rm -Rf " + str(path),
                                     {'waitForCompletion': True, 'daemonWait': 1}, None)

    def _exec_cmdln(self, cmdln, procCtrl, stdInFile):
        """Execute an OS command line (as a single string) on the remote host.
        cmdln - complete command line of the OS command
        procCtrl - procCtrl object from message which controls how the process
        is started (blocking vs non-blocking and output reporting)
        """
        _logger.debug('\nRCH--> remote_ch, exec cmdln %s', cmdln)
        check_connected(self.client)
        contents = None
        if stdInFile != None:
            with self.open(stdInFile) as stdin:
                contents = stdin.read()

        with contextlib.closing(self.client.get_transport().open_session()) as chan:
            chan.set_combine_stderr(True)
            _logger.debug('\nRCH--> cmdln=%s', cmdln)
            chan.exec_command(cmdln)

            if contents != None:
                _logger.debug('\nRCH--> Using supplied stdin from %s:', stdInFile)
                _logger.debug('\nRCH--> %s ...', contents[0:50])
                chan.sendall(contents)
                chan.shutdown_write()

            if util.get_val(procCtrl, 'waitForCompletion'):
                _logger.debug("RCH--> Has waitForCompletion")
                output = chan.makefile('rb')
                if procCtrl is not None and procCtrl.has_key('daemonWait'):
                    procDW = procCtrl['daemonWait']
                    _logger.warning(bcolors.WARNING + '\nRCH--> Waiting for command %s seconds.'
                                    + bcolors.ENDC, procDW)
                    time.sleep(procDW)
                    for t in xrange(180): #Wait MAX 180s for remote execution to return.
                        time.sleep(0.5)
                        if chan.exit_status_ready():
                            _logger.warning(bcolors.WARNING + '\nRCH--> Waited %s sec (on top '
                                            'of %s sec) for %s' + bcolors.ENDC, t/2, procDW, cmdln)
                            break
                    if chan.exit_status_ready():
                        exitstatus = chan.recv_exit_status()
                        if exitstatus != 0 and exitstatus != util.get_val(procCtrl, 'noRaise'):
                            raise RemoteExecException(self.host, cmdln, exitstatus, output)
                        out_ret = output.read()
                        if exitstatus != 0:
                            out_ret += ' errcode:' + str(exitstatus)
                        return out_ret
                    else:
                        #Timeout. RemoteExecException will not work as it assumes command completed.
                        return 'Command `{0}\', running on {1} timed out.'.format(cmdln, self.host)
                else:
                    # Block.
                    _logger.debug('\nRCH--> Waiting for %s...', cmdln)
                    exitstatus = chan.recv_exit_status()
                if exitstatus != 0 and exitstatus != util.get_val(procCtrl, 'noRaise'):
                    raise RemoteExecException(self.host, cmdln, exitstatus, output)
                out_ret = output.read()
                if exitstatus != 0:
                    out_ret += ' errcode:' + str(exitstatus)
                return out_ret
            else:
                _logger.debug("RCH--> NO waitForCompletion")
                dbg_msg = '\nRCH--> chan.exit_status_ready(1) %s'
                _logger.debug(dbg_msg, str(chan.exit_status_ready()))
                if not chan.exit_status_ready() and procCtrl is not None \
                    and procCtrl.has_key('daemonWait'):
                    procDW = procCtrl['daemonWait']
                    #Let's not waste user's time:
                    _logger.debug('\nRCH--> Waiting for command %s seconds.', procDW)
                    time.sleep(procCtrl['daemonWait'])
                    for t in xrange(180):
                        time.sleep(0.5)
                        if chan.exit_status_ready():
                            _logger.warning(bcolors.WARNING + '\nRCH--> Waited %s sec for %s'
                                            + bcolors.ENDC, t/2, cmdln)
                            break
                _logger.debug('\nRCH--> chan.exit_status_ready(2) %s',
                              str(chan.exit_status_ready()))
                if chan.exit_status_ready():
                    output = chan.makefile('rb')
                    #Strange to return with exception without checking...
                    exitstatus = chan.recv_exit_status()
                    # noRaise = 1 exclusively for installing Windows service.
                    if exitstatus != 0 and exitstatus != util.get_val(procCtrl, 'noRaise'):
                        raise RemoteExecException(self.host, cmdln, chan.recv_exit_status(), output)
                    else:
                        _logger.debug('\nRCH--> remote_ch, exec cmdln, returning')
                        out_ret = output.read()
                        if exitstatus != 0:
                            out_ret += ' errcode:' + str(exitstatus)
                        return out_ret
                else:
                    # this is for commands that we collect status for differently, say mysqld start
                    return 'Command `{0}\', running on {1} timed out.'.format(cmdln, self.host)

    def _exec_cmdv(self, cmdv, procCtrl, stdin_file):
        """Execute an OS command vector on the remote host.
        cmdv - complete command vector (argv) of the OS command
        procCtrl - procCtrl object from message which controls how the process
        is started (blocking vs non-blocking and output reporting)
        """
        _logger.debug('RCH--> remote_ch, exec cmdv')
        if isinstance(cmdv, list):
            return self._exec_cmdln(' '.join([quote_if_contains_space(a) for a in cmdv]),
                                    procCtrl, stdin_file)
        return self._exec_cmdln(cmdv, procCtrl, stdin_file)

    def execute_command(self, cmdv, inFile=None):
        """Execute an OS command blocking on the local host, using
        subprocess module. Returns dict containing output from process.
        cmdv - complete command vector (argv) of the OS command.
        inFile - File-like object providing stdin to the command.
        """
        cmdln = ' '.join([quote_if_contains_space(a) for a in cmdv])
        _logger.debug('\nRCH--> remote_ch, execute command %s', cmdln)
        check_connected(self.client)
        with contextlib.closing(self.client.get_transport().open_session()) as chan:
            chan.exec_command(cmdln)
            if inFile:
                chan.sendall(inFile.read())
                chan.shutdown_write()

            result = {
                'exitstatus': chan.recv_exit_status()
                }
            with contextlib.closing(chan.makefile('rb')) as out_file:
                result['out'] = out_file.read()

            with contextlib.closing(chan.makefile_stderr('rb')) as err_file:
                result['err'] = err_file.read(),

            return result
