# Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

"""Tools for performing tasks on (possibly remote) Cluster hosts."""

import time
import posixpath
import ntpath
import abc
import os
import socket
import subprocess
import tempfile
import shutil
import logging
import platform
import json
import stat

import util

import request_handler

_logger = logging.getLogger(__name__)


class ExecException(Exception):
    """Exception type thrown when process-spawning fails on 
    the local host. """
    
    def __init__(self, cmd, exitstatus, out):
        self.cmd = cmd
        self.exitstatus = exitstatus
        self.out = out.read()
    def __str__(self):
        return 'Command `{self.cmd}\' exited with {self.exitstatus}:\n{self.out}'.format(self=self)


class HostInfo(object):
    """Class which provides host information from a Linux-style /proc file system."""
    def __init__(self, ch, uname, machine):
        self.ch = ch
        self.pm = posixpath
        self.uname = uname
        self.machine = machine
        self.envcmd = [ 'env' ]

    @property
    def _host_info_path(self):
        return self.path_module.join(request_handler.configdir, 'host_info', 'binaries', self.uname, self.machine, 'host_info')

    def _run_host_info(self):
        hostRes = json.loads(self.ch._exec_pkg_cmdv([self._host_info_path]))
        hostRes['uname'] = self.uname
        return { 'host': { 'name' : self.ch.host }, 'hostRes': hostRes } 
    
#     def _get_hostInfo(self):
#         meminfo = self.ch.open('/proc/meminfo')
#         cpuinfo = self.ch.open('/proc/cpuinfo')
#         try:
#             return { 'host': { 'name' : self.ch.host }, 
#                      'hostRes': { 'ram' : int(meminfo.readline().split()[1]) / 1024, 
#                                   'cores': len([ln for ln in cpuinfo.readlines() if 'processor' in ln]),
#                                   'uname': self.uname}}                     
#         finally:
#             meminfo.close()
#             cpuinfo.close()

    @property
    def ram(self):
        """Caching?"""
        with self.ch.open('/proc/meminfo') as meminfo:
            return int(meminfo.readline().split()[1]) / 1024

    @property
    def cores(self):
        """x"""
        with self.ch.open('/proc/cpuinfo') as cpuinfo:
            return len([ln for ln in cpuinfo.readlines() if 'processor' in ln])
        
    @property
    def installdir(self):
        """x"""
        return None # Don't know how to get this for remote hosts

    @property
    def homedir(self):
        """x"""
        return self.ch.env['HOME']

#     @property
#     def rep(self):
#         """A Python dict representation of the hostInfoRep structure which can be directly converted to Json."""
#         reply = None
# #         try:
# #             reply = self._run_host_info()
# #         except:
# #             _logger.exception('Running host_info failed. Falling back to system tools:')
        
#         reply = self._get_hostInfo()

#         local = platform.system()
#         remote = self.uname
#         if (local == 'Windows' and (remote == 'CYGWIN' or remote == 'Windows')) or (local != 'Windows' and remote != 'CYGWIN' and remote != 'Windows'):
#             _logger.debug('localhost and remote are similar type, apply local paths')
#             _logger.debug('remote env: '+str(self.ch.env))
#             reply['hostRes']['installdir'] = request_handler.basedir
#             reply['hostRes']['datadir'] = self.pm.join(self.pm.expanduser('~'), 'MySQL_Cluster')

#         return reply
    
    @property
    def path_module(self):
        """Returns the python path module to use when manipulating path names on this host."""
        return self.pm

     
class  SolarisHostInfo(HostInfo):
    """Specialization for Solaris which uses prtconf and psrinfo to retrieve host information."""

#     def _get_hostInfo(self):
#         return { 'host': { 'name' : self.ch.host }, 
#                 'hostRes': { 'ram' :  int(self.ch.exec_blocking(['/usr/sbin/prtconf']).split()[7]), 
#                             'cores': len(self.ch.exec_blocking(['/usr/sbin/psrinfo']).split('\n')[0:-1]),
#                             'uname': self.uname}}
    @property
    def ram(self):
        return int(self.ch.exec_blocking(['/usr/sbin/prtconf']).split()[7])

    @property
    def cores(self):
        return len(self.ch.exec_blocking(['/usr/sbin/psrinfo']).split('\n')[0:-1])

class MacHostInfo(HostInfo):
    """Specialization for MacOS which uses sysctl to retrieve host information."""

#     def _get_hostInfo(self):
#         sysinfo = self.ch.exec_blocking(['/usr/sbin/sysctl', 'hw.']) 
#         ram = [int(filter(str.isdigit, ln.split()[1])) for ln in sysinfo.split('\n') if 'hw.memsize:' in ln][0] / 1024 / 1024
#         cores = [int(filter(str.isdigit, ln.split()[1])) for ln in sysinfo.split('\n') if 'hw.ncpu:' in ln][0] 
#         return { 'host': { 'name' : self.ch.host },
#                   'hostRes': { 'ram' : ram, 'cores': cores,
#                                'uname': self.uname}}                    
    
    @property
    def ram(self):
        sysinfo = self.ch.exec_blocking(['/usr/sbin/sysctl', 'hw.'])
        return [int(filter(str.isdigit, ln.split()[1])) for ln in sysinfo.split('\n') if 'hw.memsize:' in ln][0] / 1024 / 1024

    @property
    def cores(self):
        sysinfo = self.ch.exec_blocking(['/usr/sbin/sysctl', 'hw.'])
        return [int(filter(str.isdigit, ln.split()[1])) for ln in sysinfo.split('\n') if 'hw.ncpu:' in ln][0]
 
class CygwinHostInfo(HostInfo):
    """Specialization for Windows Cygwin which uses systeminfo and wmic to retrieve host information, but retains posixpath as the path module."""

    @property
    def _host_info_path(self):
        return self.path_module.join('install','host_info', 'Windows', 'host_info.exe')


#     def _get_hostInfo(self):
#         sysinfo = self.ch.exec_blocking(['C:/Windows/system32/systeminfo'])
#         ram = [ int(filter(str.isdigit, ln.split()[3])) for ln in sysinfo.split('\n') if 'Total Physical Memory:' in ln][0]
#         self.logger.debug("ram="+str(ram))
        
#         wmic = self.ch.exec_blocking(['C:/Windows/System32/Wbem/wmic', 'CPU', 'GET', '/VALUE'])
#         if isinstance(self.ch, LocalClusterHost):
#             wmic = unicode(wmic, 'utf-16')

#         cores = sum([ int(ln.split('=')[1]) 
#                       for ln in wmic.split('\n') if  'NumberOfCores' in ln ])
#         self.logger.debug("cores="+str(cores))
#         return { 'host': { 'name' : self.ch.host }, 
#                'hostRes': { 'ram' : ram, 'cores': cores, 'uname': self.uname}}

    @property
    def ram(self):
        sysinfo = self.ch.exec_blocking(['C:/Windows/system32/systeminfo'])
        return [ int(filter(str.isdigit, ln.split()[3])) for ln in sysinfo.split('\n') if 'Total Physical Memory:' in ln][0]

    @property
    def cores(self):
        wmic = self.ch.exec_blocking(['C:/Windows/System32/Wbem/wmic', 'CPU', 'GET', '/VALUE'])
        if isinstance(self.ch, LocalClusterHost):
            wmic = unicode(wmic, 'utf-16')

        return sum([ int(ln.split('=')[1]) for ln in wmic.split('\n') if  'NumberOfCores' in ln ])

class WindowsHostInfo(CygwinHostInfo):
    """Specialization of CygwinHostInfo for native Windows which uses ntpath as the path module."""

    def __init__(self, ch, uname, machine):
        super(type(self), self).__init__(ch, uname, machine)
        self.pm = ntpath
        self.envcmd = [ 'cmd.exe', '/c', 'set' ]

    @property
    def cores(self):
        return int(self.ch.env['NUMBER_OF_PROCESSORS'])

    @property
    def homedir(self):
        env = self.ch.env
        if env.has_key('USERPROFILE'):
            return env['USERPROFILE']
        
        return env['HOMEDRIVE']+env['HOMEPATH']
            

    @property
    def path_module(self):
        return ntpath

# Map from uname string to HostInfo type    
hostInfo_map = { 'SunOS' : SolarisHostInfo,
                 'Darwin' : MacHostInfo,
                 'Windows' : WindowsHostInfo,
                 'CYGWIN' : CygwinHostInfo }
    
class ABClusterHost(object):
    """Base class providing common interface."""
    __meta_class__  = abc.ABCMeta

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.drop()
        return False

    def __init__(self):
        self._hostInfo = None
        self._env = None
        
    @abc.abstractmethod
    def _get_system_tuple(self):
        """Return a string identifying the hosts operating system."""
        pass
    
    @abc.abstractmethod
    def _exec_pkg_cmdv(self, cmdv):
        """Execute a package binary on this ClusterHost."""
        pass
    

    @abc.abstractmethod
    def open(self, filename, mode='r'):
        pass


    
    @property
    def hostInfo(self):
        """Create an appropriate HostInfo object for localhost. 
        Uses Pythons platform.system() to determine the type of HostInfo object to return. 
        """
        if self._hostInfo == None:
            (system, machine) = self._get_system_tuple()
            if hostInfo_map.has_key(system):
                self._hostInfo = hostInfo_map[system](self, system, machine)
            else:
                _logger.debug('Using default HostInfo for host='+self.host+'('+system+','+machine+')')
                return HostInfo(self, system, machine)

        return self._hostInfo
    
    @property
    def path_module(self):
        """Path module to use when manipulating file paths."""
        return self.hostInfo.path_module

    @property
    def env(self):
        if not self._env:
            ctx = { 'str': self.exec_blocking(self.hostInfo.envcmd), 'properties': {} }
            util.parse_properties(ctx)
            self._env = ctx['properties']

        return self._env

    @property
    def ram(self):
        return self.hostInfo.ram

    @property
    def cores(self):
        return self.hostInfo.cores

    @property
    def uname(self):
        return self.hostInfo.uname

    @property
    def installdir(self):
        return self.hostInfo.installdir

    @property
    def homedir(self):
        return self.hostInfo.homedir
 
   
    @abc.abstractmethod    
    def drop(self, paths=[]):
        """Close open connections and remove files.
        paths - list of files to remove from host before closing connection
        """
        map(self.rm_r, paths)
    
    @abc.abstractmethod    
    def file_exists(self, path):
        """Test for the existence of a file. If the file actually exists,
        its stat object is returned, otherwise None.
        path - file to check the existence of
        """
        pass
    
    @abc.abstractmethod
    def list_dir(self, path):
        """List the files in a directory. 
        path - directory to list
        """
        pass

    @abc.abstractmethod
    def mkdir_p(self, path):
        """Provides mkdir -p type functionality. That is,
        all missing parent directories are also created. If the directory we are trying to 
        create already exists, we silently do nothing. If path or any of its parents is not
        a directory an exception is raised.
        path - directory to create on remote host
        """
        pass

    @abc.abstractmethod       
    def rm_r(self, path):
        """Provides rm -r type functionality. That is, all files and 
        directories are removed recursively.
        path - file or directory to remove
        """
        pass
    
    def auto_complete(self, basedir, locations, executable):
        """Find the absolute path of an executable given a prefix directory, a set 
        of possible directories, and the basename of the executable.
        basedir - basedir of a cluster installation
        locations - list of directories to try
        executable - basename of executable to auto-complete
        """
        for l in locations:
            choice = posixpath.join(basedir,l,executable)
            if self.file_exists(choice):
                return choice
        raise Exception('Cannot locate '+executable+' in '+posixpath.join(basedir, str(locations))+' on host '+self.host)
   
    @abc.abstractmethod
    def _exec_cmdv(self, cmdv, procCtrl, stdinFile):
        pass

    def exec_cmdv(self, cmdv, procCtrl={ 'waitForCompletion': True }, 
                  stdinFile=None):
        """Forwards to virtual."""
        return self._exec_cmdv(cmdv, procCtrl, stdinFile)
                
    def exec_blocking(self, cmdv):
        """Convenience method."""
        assert(isinstance(cmdv, list))
        return self.exec_cmdv(cmdv, { 'waitForCompletion': True })
    
    def exec_cluster_daemon(self, cdaemonv, waitsec):
        """Convenience method."""
        assert(isinstance(cdaemonv, list))
        return self.exec_cmdv(cdaemonv, { 'daemonWait': waitsec })
        
            

class LocalClusterHost(ABClusterHost):
    """Implement the ABClusterHost interface for access to the local host without 
    using SSH over Paramiko. Note that this implies that there will be no authentication """
    
    def __init__(self, host):
        super(type(self), self).__init__()
        self.host = host

    def _get_system_tuple(self):
        system = platform.system()
        if system == 'Windows':
            try:
                subprocess.check_call(['uname'])
            except:
                _logger.debug('No uname available, assuming native Windows')
                return (system, platform.uname()[-2])
            else:
                return ('CYGWIN', 'Unknown')            
        return (system, platform.uname()[-1])

    def _exec_pkg_cmdv(self, cmdv):
        """Locally this just forwards to exec_cmdv."""
        return self.exec_cmdv(cmdv)

    @property
    def env(self):
        return os.environ

    @property
    def cores(self):
        return self.hostInfo.cores

    @property
    def installdir(self):
        return request_handler.basedir

    def open(self, filename, mode='r'):
        """Open a file on ABClusterHost."""
        return open(filename, mode)
    
    def drop(self, paths=[]):
        """Close open connections and remove files.
        paths - list of files to remove from host before closing connection
        """
        map(self.rm_r, paths)
        
    def file_exists(self, path):
        """Test for the existence of a file on the local host. If the file actually exists,
        its stat result object is returned, otherwise None.
        path - file to check the existence of
        """
        if os.path.exists(path):
            return os.stat(path)
        else:
            return None

    def list_dir(self, path):
        """List the files in a directory on the local host. Forwards to os.listdir().
        path - directory to list
        """
        return os.listdir(path)
    
    def mkdir_p(self, path):
        """Provides mkdir -p type functionality on the local host. Does nothing if 
        the directory already exists, otherwise forwards to os.makedirs
        path - directory to create on remote host
        """
        if os.path.exists(path) and os.path.isdir(path):
            return
        os.makedirs(path)
           
    def rm_r(self, path):
        """Provides rm -r type functionality on the local host. Forwards to os.rmdirs.
        path - file or directory to remove
        """
        shutil.rmtree(path)
               
    def _exec_cmdv(self, cmdv, procCtrl, stdinFile):
        """Execute an OS command on the local host, using subprocess module.
        cmdv - complete command vector (argv) of the OS command
        procCtrl - procCtrl object from message which controls how the process
        is started (blocking vs non-blocking and output reporting)
        """
        
        # Add nohup when running locally to prevent server from
        # waiting on the children
        if util.get_val(procCtrl, 'nohup'):
            cmdv[:0] = ['nohup']
        output = tempfile.TemporaryFile()
        stdin = output
        if (stdinFile != None):
            stdin = self.open(stdinFile)

        try:
            if util.get_val(procCtrl, 'waitForCompletion'):
                try:
                    subprocess.check_call(cmdv, stdin=stdin, stdout=output, stderr=output)
                except subprocess.CalledProcessError as cpe:
                    if cpe.returncode != util.get_val(procCtrl, 'noRaise', 0):
                        output.seek(0)
                        _logger.error('output='+output.read())
                        raise
                output.seek(0)
                return output.read()
            
            proc = subprocess.Popen(cmdv, stdin=stdin, stderr=output)
            if proc.poll() == None and procCtrl.has_key('daemonWait'):
                _logger.debug('Popen waits {0} sec for {1}'.format(procCtrl['daemonWait'], ' '.join(cmdv)))
                time.sleep(procCtrl['daemonWait'])
            if proc.poll() != None:
                output.seek(0)
                raise ExecException(' '.join(cmdv), proc.returncode, output)
        finally:
            #output.seek(0)
            #print output.read()
            if (stdin != output):
                stdin.close()
            output.close()

    def execute_command(self, cmdv, inFile=None):
        """Execute an OS command blocking on the local host, using 
        subprocess module. Returns dict contaning output from process. 
        cmdv - complete command vector (argv) of the OS command.
        """
        outFile = tempfile.TemporaryFile()
        errFile = tempfile.TemporaryFile()
        result = {
            'exitstatus': subprocess.call(args=cmdv, stdin=inFile, 
                                          stdout=outFile, stderr=errFile) 
            }
        outFile.seek(0)
        errFile.seek(0)
        result['out'] = outFile.read()
        result['err'] = errFile.read()
        return result

def produce_ABClusterHost(hostname='localhost', user=None, pwd=None):
    """Factory method which returns RemoteClusterHost or LocalClusterHost depending 
    on the value of hostname.."""

    if hostname == 'localhost' or hostname == '127.0.0.1' or hostname == socket.gethostname():
        return LocalClusterHost(hostname)
    
    hostname_fqdn = socket.getfqdn(hostname)
    if hostname_fqdn == socket.getfqdn('localhost') or hostname_fqdn == socket.getfqdn(socket.gethostname()):
        return LocalClusterHost(hostname)
    import remote_clusterhost
    return remote_clusterhost.RemoteClusterHost(hostname, user, pwd)
