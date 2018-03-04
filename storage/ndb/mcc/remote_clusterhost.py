# Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

"""Provides specialization of ABClusterHost for remote hosts using Paramiko."""

import errno
import stat
import util
import time
import paramiko
import ntpath
import logging
import os.path
import tempfile
import contextlib
import posixpath
import cStringIO
import base64

import clusterhost
from clusterhost import ABClusterHost

_logger = logging.getLogger(__name__)

def quote_if_contains_space(s):
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
        return 'Command `{self.cmd}\', running on {self.hostname} exited with {self.exitstatus}:\n{self.out}'.format(self=self)

class RemoteClusterHost(ABClusterHost):
    """Implements the ABClusterHost interface for remote hosts. Wraps a paramiko.SSHClient and uses
    this to perform tasks on the remote host."""

    def __init__(self, host, key_based=None, username=None, password=None, key_file=None):
        super(type(self), self).__init__()
        self.host = host

        if username is not None:
            _logger.warning('--> ' + "username="+username)
        if password is not None:
            if password.startswith('**'):
                password=None
            else:
                _logger.warning('--> ' + "pwd=**")
        if key_based is not None:
            _logger.warning('--> ' + "keybased="+str(key_based))
        if key_file is not None:
            _logger.warning('--> ' + "keyfile="+key_file)
        _logger.warning('--> ' + "Host="+host)
        self.user = username
        self.pwd = password
        c = paramiko.SSHClient()
        c.load_system_host_keys()
        c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        if key_based is False:
            _logger.warning('--> ' + "Enter 1")
            c.connect(hostname=self.host, username=self.user, password=self.pwd)
        else:
            if (key_file is None):
                _logger.warning('--> ' + "Enter 2")
                c.connect(hostname=self.host, username=self.user, password=self.pwd)
            else:
                #We have key file pointed out for us.
                _logger.warning('--> ' + "Enter 4")
                #privatekeyfile = os.path.expanduser('~/.ssh/id_rsa')
                mykey = paramiko.RSAKey.from_private_key_file(key_file)
                c.connect(hostname=self.host, username=self.user, password=self.pwd, pkey=mykey)
            
        self.__client = c
        self.__sftp = c.open_sftp()

    def close(self):
        self.drop()

    @property
    def client(self):
        return self.__client

    @property
    def sftp(self):
        return self.__sftp
    
#     @property
#     def client(self):
#         """"A freshly connected SSHClient object."""
#         if self.__client != None:
#             if self.__sftp != None:
#                 self.__sftp.close()
#                 self.__sftp = None
#             self.__client.close()

#         c = paramiko.SSHClient()
#         c.load_system_host_keys()

#         # TODO - we need user acceptance for this by button in the frontend
#         c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
#         ak = 'H:\\.ssh\\known_hosts'
#         if os.path.exists(ak):
#             _logger.debug('Loading additional host keys from %s', ak)
#             c.load_host_keys(filename=ak)
#         else:
#             _logger.debug('File %s does not exist here', ak)

#         c.connect(hostname=self.host, username=self.user, password=self.pwd)
#         self.__client = c
#         return c

#     @property
#     def sftp(self):
#         """"An SFTPClient object to this host. It and its SSHClient 
#         object will be created on demand."""
#         if self.__sftp != None:
#             self.__sftp.close()

#         self.__sftp = self.client.open_sftp()
#         return self.__sftp

    def _get_system_tuple(self):
        preamble = None
        system = None
        processor = None
        try:
            preamble = self.exec_blocking(['uname', '-sp']) #'#'])
        except:
            _logger.debug('executing uname failed - assuming Windows...')
            (system, processor) = self.exec_blocking(['cmd.exe', '/c', 'echo', '%OS%', '%PROCESSOR_ARCHITECTURE%']).split(' ')
            if 'Windows' in system:
                system = 'Windows'
        else:
            lc = preamble.count('\n')
            if lc > 1: #There was a prior error which occupies space on top of uname output.
                preamble = preamble.split('\n')[lc-1]
                
            uname = preamble
            _logger.debug('uname='+uname)
            (system, processor) = uname.split(' ')
            if 'CYGWIN' in system:
                system = 'CYGWIN'
        return (system, processor.strip())

    def _exec_pkg_cmdv(self, cmdv):
        """For remote hosts the binary is fist copied over using sftp."""
        _logger.debug("%s", str(self.sftp.listdir()))
        hi = os.path.basename(cmdv[0])
        self.sftp.put(cmdv[0], hi)
        self.sftp.chmod(hi, stat.S_IRWXU)
        return self.exec_cmdv([self.path_module.join('.', hi)] + cmdv[1:-1])

    def _sftpify(self, path):
        """Since sftp treats all path names as relative to its root we must 
        convert absolute paths before using them with sftp. As quick-fix we 
        assume that the sftp root is equal to the drive letter of all absolute 
        paths used. If it isn't the sftp operations will fail."""
        return self.path_module.splitdrive(path)[1]

    def open(self, filename, mode='r'):
        """Forward to paramiko.SFTPClient.open for remote hosts.
        Wrap in contextlib.closing so that clients can use 
        with-statements on it."""
        return contextlib.closing(self.sftp.open(self._sftpify(filename), mode))
        
    def drop(self, paths=[]):
        """Close open connections and remove files.
        paths - list of files to remove from host before closing connection
        """
        map(self.rm_r, paths)
        if self.__sftp:
            self.__sftp.close()
            self.__sftp = None
        if self.__client:
            self.__client.close()
            self.__client = None
        
    def file_exists(self, path):
        """Test for the existence of a file on the remote host. If the file actually exists,
        its stat object is returned, otherwise None.
        path - file to check the existence of
        """
        try:
            return self.sftp.stat(self._sftpify(path))
        except IOError as ioerr:
            if ioerr.errno == errno.ENOENT:
                return None
            _logger.debug('stat failure on '+path)
            raise
    
    def list_dir(self, path):
        """List the files in a directory on the remote host. Forwards to 
        SFTPClient.listdir(), but also warns about empty results that may be caused
        by paramiko not reporting missing execute permission on the directory 
        correctly.
        path - directory to list
        """
        content = self.sftp.listdir(self._sftpify(path))
        if len(content) == 0:
            m = stat.S_IMODE(self.sftp.stat(path).st_mode)
            for role in ['USR', 'GRP', 'OTH']:
                mask = util.get_fmask('R', role)|util.get_fmask('X',role)
                if (m & mask) != mask:
                    _logger.debug('Directory '+path+' does not have both read and execute permission for ' + role + '.\nIf you depend on '+role+ ' for access, the empty directory listing may not be correct')
        
        return content
    
    def mkdir_p(self, path):
        """Provides mkdir -p type functionality on the remote host. That is,
        all missing parent directories are also created. If the directory we are trying to 
        create already exists, we silently do nothing. If path or any of its parents is not
        a directory an exception is raised.
        path - directory to create on remote host
        """
        _logger.debug('mkdir_p('+path+')')
        path = self._sftpify(path)
        pa = self.file_exists(path)
        if pa != None:
            #print str(pa)+" "+str(pa.st_mode)
            if not util.is_dir(pa):
                raise Exception(self.host+':'+path+' is not a directory')
            return
        # Need to user normpath here since dirname of a directory with a trailing slash
        #  is the directory without a slash (a dirname bug?)
        sd = ntpath.splitdrive(path)
        _logger.debug('sd='+str(sd))
        if sd[1] == '':
            _logger.debug('path='+path+' is a drive letter. Returning...')
            return
            
        np = self.path_module.normpath(path)
        parent = self.path_module.dirname(np)
        assert parent != path
        self.mkdir_p(parent)
        self.sftp.mkdir(np)
           
    def rm_r(self, path):
        """Provides rm -r type functionality on the remote host. That is, all files and 
        directories are removed recursively.
        path - file or directory to remove
        """
        path = self._sftpify(path)
        if util.is_dir(self.sftp.stat(path)):
            for f in self.sftp.listdir(path):
                self.rm_r(self.posixpath.join(path,f))
            self.sftp.rmdir(path)
        else:
            self.sftp.remove(path)
               
    def _exec_cmdln(self, cmdln, procCtrl, stdinFile):
        """Execute an OS command line (as a single string) on the remote host.
        cmdln - complete command line of the OS command
        procCtrl - procCtrl object from message which controls how the process
        is started (blocking vs non-blocking and output reporting)
        """

        contents = None
        if (stdinFile != None):
            with self.open(stdinFile) as stdin:
                contents = stdin.read()

        with contextlib.closing(self.client.get_transport().open_session()) as chan:
            chan.set_combine_stderr(True)
            _logger.debug('cmdln='+cmdln)
            chan.exec_command(cmdln)
    
            if (contents != None):
                _logger.debug('Using supplied stdin from ' + stdinFile + ': ')
                _logger.debug(contents[0:50] + '...')
                chan.sendall(contents)
                chan.shutdown_write()     

            if util.get_val(procCtrl, 'waitForCompletion'):
                output = chan.makefile('rb')
                _logger.debug('Waiting for command...')
                exitstatus = chan.recv_exit_status()
                if exitstatus != 0 and exitstatus != util.get_val(procCtrl, 'noRaise'): 
                    raise RemoteExecException(self.host, cmdln, exitstatus, output)
                return output.read()
            else:
                if not chan.exit_status_ready() and procCtrl.has_key('daemonWait'):
                    _logger.debug('Waiting {0} sec for {1}'.format(procCtrl['daemonWait'], cmdln))
                    time.sleep(procCtrl['daemonWait'])
                if chan.exit_status_ready():
                    output = chan.makefile('rb')
                    raise RemoteExecException(self.host, cmdln, chan.recv_exit_status(), output)
            
    def _exec_cmdv(self, cmdv, procCtrl, stdinFile):
        """Execute an OS command vector on the remote host.
        cmdv - complete command vector (argv) of the OS command
        procCtrl - procCtrl object from message which controls how the process
        is started (blocking vs non-blocking and output reporting)
        """

        assert isinstance(cmdv, list)
        return self._exec_cmdln(' '.join([quote_if_contains_space(a) for a in cmdv]), procCtrl, stdinFile)


    def execute_command(self, cmdv, inFile=None):
        """Execute an OS command blocking on the local host, using 
        subprocess module. Returns dict contaning output from process. 
        cmdv - complete command vector (argv) of the OS command.
        inFile - File-like object providing stdin to the command.
        """
        cmdln = ' '.join([quote_if_contains_space(a) for a in cmdv])
        _logger.debug('cmdln='+cmdln)

        with contextlib.closing(self.client.get_transport().open_session()) as chan:
            chan.exec_command(cmdln)
            if inFile:
                chan.sendall(inFile.read())
                chan.shutdown_write()     

            result = {
                'exitstatus': chan.recv_exit_status()
                }
            with contextlib.closing(chan.makefile('rb')) as outFile:
                result['out'] = outFile.read()

            with contextlib.closing(chan.makefile_stderr('rb')) as errFile:
                result['err'] = errFile.read(),

            return result

