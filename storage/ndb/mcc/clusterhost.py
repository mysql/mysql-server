#Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
#
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License, version 2.0,
#as published by the Free Software Foundation.
#
#This program is also distributed with certain software (including
#but not limited to OpenSSL) that is licensed under separate terms,
#as designated in a particular file or component or in included license
#documentation.  The authors of MySQL hereby grant you an additional
#permission to link the program and your derivative works with the
#separately licensed software that they have included with MySQL.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License, version 2.0, for more details.
#
#You should have received a copy of the GNU General Public License along with this program; if not,
#write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

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
import shlex

import util
from util import bcolors

import request_handler

_logger = logging.getLogger(__name__)
# Lock for executing commands fast on localhost.
# CMD_LOCK = threading.Lock()
class ExecException(Exception):
    """Exception type thrown when process-spawning fails on the local host. """

    def __init__(self, cmd, exitstatus, out):
        self.cmd = cmd
        self.exitstatus = exitstatus
        self.out = out.read()
    def __str__(self):
        return bcolors.FAIL + 'Command ' + self.cmd + ' exited with ' + self.exitstatus + ':\n' + \
            self.out + bcolors.ENDC.format

class HostInfo(object):
    """Class which provides host information from a Linux-style /proc file system."""
    def __init__(self, ch, uname, machine, osver, osflavor):
        self.ch = ch
        self.pm = posixpath
        self.uname = uname
        self.machine = machine
        self.envcmd = ['env']
        self.osver = osver
        self.osflavor = osflavor

    @property
    def _host_info_path(self):
        return self.path_module.join(request_handler.CONFIGDIR, 'host_info', 'binaries', self.uname,
                                     self.machine, 'host_info')

    def _run_host_info(self):
        host_res = json.loads(self.ch._exec_pkg_cmdv([self._host_info_path]))
        host_res['uname'] = self.uname
        return {'host': {'name' : self.ch.host}, 'hostRes': host_res}

    @property
    def ram(self):
        """Caching?"""
        with self.ch.open('/proc/meminfo') as meminfo:
            return int(meminfo.readline().split()[1]) // 1024

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

    @property
    def path_module(self):
        """Returns the python path module to use when manipulating path names on this host."""
        return self.pm

    @property
    def disk_free(self):
        """Returns the free space for homedir on host."""
        hd = self.ch.env['HOME']
        dtest = self.ch.exec_blocking(['df', '-h', hd])

        #There is a chance there will be an error output from opening the console.
        # grab df output, skip to last line, go one back and grab Avail (4th).
        lc = dtest.count('\n')
        #There has to be 2 lines of df output + errors from console (if any).
        if lc > 1:
            ds = str(((dtest).split('\n')[lc-1]).split()[3:4]).strip('[]').strip("''")
            if len(ds) >= 2:
                return ds
            wrn_msg = 'CHS--> Length of parsed df output was less than 2 characters wide.'
            _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC)
            return 'unknown'
        else:
            wrn_msg = 'CHS--> df output had less than 2 lines.'
            _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC)
            return 'unknown'

    @property
    def docker_info(self):
        """Returns NOT INSTALLED, NOT RUNNING or RUNNING for Docker."""
        # We will ask for this info if necessary. IF docker is requested and it's NOT INSTALLED, we
        # will install and run docker. IF docker NOT RUNNING we will start it. IF docker is RUNNING,
        # front-end will do nothing. All *nix platforms share same code.
        d_test = None
        try:
            d_test = self.ch.exec_blocking(['docker', '-v'])
            _logger.debug('CHS--> docker -v finished with dtest = %s', d_test)
        except:
            _logger.debug('CHS--> docker -v failed with dtest = %s', d_test)
            d_test = ""
        #We ignore ver. info for now and send new request for result of command.
        if d_test != "":
            #Now check if docker is running.
            try:
                d_test = self.ch.exec_blocking(['ps', '-e', '-o', 'comm', '|', 'grep', '[d]ocker'])
                _logger.debug('CHS--> ps finished with dtest = %s', d_test)
            except:
                wrn_msg = 'CHS--> ps failed with dtest = %s'
                _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, d_test)
                d_test = ""
            if d_test != "":
                return 'RUNNING'
            return 'NOT RUNNING'
        return 'NOT INSTALLED'

    @property
    def intip(self):
        """Returns the internal IP address of the host. Should work for all but Mac and Win."""
        _logger.debug('CHS--> Running ifconfig')
        d_test = None
        if isinstance(self.ch, LocalClusterHost):
            # localhost providing addressed via external IP can still be part of Cluster
            # requiring InternalIP
            d_test = subprocess.Popen([
                r'ifconfig -a | grep -A 1 "RUNNING" | grep -v "LOOPBACK" | grep -v "127\.0\.0\.1"' +
                r' | grep -oP "(?<=inet\s)\d+(\.\d+){3}"'], stdout=subprocess.PIPE, shell=True)
            dt = d_test.communicate()[0]
            return str(dt).split("\n")
        else:
            d_test = self.ch.exec_blocking(
                ['/usr/sbin/ifconfig', '-a', '|', 'grep', '-A', '1', '"RUNNING"', '|', 'grep', \
                 '-v', '"LOOPBACK"', '|', 'grep', '-v', r'"127\.0\.0\.1"', \
                 '|', 'grep', '-oP', r'"(?<=inet\s)\d+(\.\d+){3}"'])

            if d_test is None:
                _logger.warning(bcolors.WARNING + 'CHS--> ifconfig failed.' + bcolors.ENDC)
                return ''
            lc = d_test.count('\n')
            # Oracle cloud machines return single internal IP address! There is 1+ ip
            # addresses, return that and let FE sort things out.
            if lc >= 1:
                return d_test
            wrn_msg = 'CHS--> ifconfig output had less than 2 lines.'
            _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC)
            return ''

class  SolarisHostInfo(HostInfo):
    """Specialization for Solaris which uses prtconf and psrinfo to get host information."""

    @property
    def ram(self):
        return int(self.ch.exec_blocking(['/usr/sbin/prtconf']).split()[7])

    @property
    def cores(self):
        return len(
            self.ch.exec_blocking(['/usr/sbin/prtconf']).split('\n')[0:-1])

    @property
    def intip(self):
        return "unknown"

class MacHostInfo(HostInfo):
    """Specialization for MacOS which uses sysctl to get host information."""

    @property
    def ram(self):
        sysinfo = self.ch.exec_blocking(['/usr/sbin/sysctl', 'hw.'])
        return [int(filter(str.isdigit, ln.split()[1])) for ln \
            in sysinfo.split('\n') if 'hw.memsize:' in ln][0] // 1024 // 1024

    @property
    def cores(self):
        sysinfo = self.ch.exec_blocking(['/usr/sbin/sysctl', 'hw.'])
        return [int(filter(str.isdigit, ln.split()[1])) for ln in \
            sysinfo.split('\n') if 'hw.ncpu:' in ln][0]

    @property
    def intip(self):
        d_test = None
        dt = []

        if isinstance(self.ch, LocalClusterHost):
            _logger.debug('CHS--> ifconfig local Mac')
            d_test = subprocess.Popen(
                ["ifconfig -a | grep -A 4 'RUNNING' | grep 'inet' | grep -v" +
                 r" 'LOOPBACK' | grep -v '127\.0\.0\.1' | grep -v 'inet6' | " +
                 "awk '''{ print $2 }'''"], stdout=subprocess.PIPE, shell=True)
            dt = d_test.communicate()[0]
            return str(dt).split("\n")
        else:
            d_test = self.ch.exec_blocking(
                ['ifconfig', '-a', '|', 'grep', '-A', '4', '"RUNNING"', '|', 'grep', '"inet"',\
                 '|', 'grep', '-v', '"LOOPBACK"', '|', 'grep', '-v', r'"127\.0\.0\.1"', '|',\
                 'grep', '-v', '"inet6"'])
            if d_test is None:
                _logger.warning(bcolors.WARNING + 'CHS--> ifconfig failed.' + bcolors.ENDC)
                return ''
            lc = d_test.count('\n')
            dt1 = []
            if lc >= 1:
                dt1 = d_test.split('\n')
                for x in xrange(len(dt1)):
                    if 'inet ' in dt1[x]:
                        _logger.debug('CHS--> Appending intIP:%s',
                                      dt1[x].lstrip().split('inet ')[1].split(' ')[0])
                        dt.append(dt1[x].lstrip().split('inet ')[1].split(' ')[0])
                return dt
            wrn_msg = 'CHS--> ifconfig output had less than 2 lines.'
            _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC)
            return ''
class CygwinHostInfo(HostInfo):
    """Specialization for Cygwin which uses systeminfo and wmic to retrieve host information, but
     retains posixpath as the path module."""

    @property
    def _host_info_path(self):
        return self.path_module.join('install', 'host_info', 'Windows', 'host_info.exe')

    @property
    def ram(self):
        cmdv = ['cmd.exe', '/c', 'wmic', 'ComputerSystem', 'get', 'TotalPhysicalMemory', '/value']
        rd = self.ch.exec_blocking(cmdv).split("\r") # TotalPhysicalMemory=34190766080
        ram = ""
        for t in rd:
            if t.strip() != "":
                if t.find("TotalPhysicalMemory") != -1:
                    ram = t.split("TotalPhysicalMemory=")[1]
                    break
        return int(ram) // 1024 // 1024 # Return MB same as systeminfo would.

    @property
    def cores(self):
        """Returns number of logical CPU's available on box."""
        wmic = self.ch.exec_blocking(['C:/Windows/System32/Wbem/wmic', 'CPU', 'GET', '/VALUE'])
        return sum(
            [int(ln.split('=')[1]) for ln in wmic.split('\n') if 'NumberOfLogicalProcessors' in ln])

    @property
    def docker_info(self):
        """Returns state (NOT INSTALLED, NOT RUNNING and RUNNING) of docker."""
        cmdv = ['C:/Windows/System32/sc.exe', 'queryex', 'com.docker.service']
        # This one is expected to fail mostly with 1060:
        # The specified service does not exist as an installed service.
        d_test = self.ch.exec_cmdv(cmdv, {'noRaise': 1060, 'waitForCompletion': True}, None)
        if len(d_test) > 10:
            if d_test.find("STATE") != -1:
                gds = d_test.split("STATE              :")[1].split("WIN32_EXIT_CODE")[0].strip()
                if gds.find("4  RUNNING") != -1:
                    return "RUNNING"
                return "NOT RUNNING"
            else:
                return "NOT INSTALLED"
        else:
            # Actually this is error in sc but the result is the same.
            return 'NOT INSTALLED(error)'

    @property
    def intip(self):
        mt = self.ch.exec_blocking(['C:/Windows/System32/ipconfig', '/all'])
        if len(mt) > 6:
            tmp = [ln for ln in mt.split("\r\n") if "IPv4" in ln]
            return [ln.split(":")[1].replace("(Preferred)", "").strip() for ln in tmp]
        return "unknown"

class WindowsHostInfo(CygwinHostInfo):
    """Specialization of CygwinHostInfo for native Windows which uses ntpath as the path module."""

    def __init__(self, ch, uname, machine, osver, osflavor):
        super(type(self), self).__init__(ch, uname, machine, osver, osflavor)
        self.pm = ntpath
        self.envcmd = ['cmd.exe', '/c', 'set']

    @property
    def homedir(self):
        en = self.ch.env
        if en.has_key('USERPROFILE'):
            return en['USERPROFILE']
        return en['HOMEDRIVE']+en['HOMEPATH']

    @property
    def path_module(self):
        return ntpath

    @property
    def disk_free(self):
        en = self.ch.env
        if en.has_key('USERPROFILE'):
            hd = en['USERPROFILE']
        else:
            hd = en['HOMEDRIVE']+en['HOMEPATH']

        mt = (self.ch.exec_blocking(['C:/Windows/System32/Wbem/wmic', 'logicaldisk', hd[:2],
                                     'GET', 'freespace'])).split('\n')[1]
        s = ""
        for i in range(len(mt)-1):
            if ord(mt[i]) >= 48:
                s += str(mt[i])
        if len(s) < 2:
            return 'unknown'
        try:
            return str(int(s) // 1024 // 1024 // 1024) + 'G'
        except (TypeError, ValueError): # hope that's all
            return 'unknown'

    @property
    def intip(self):
        """ Returns (Array of) internal IP address(es)."""
        # This should be it:
        mt = self.ch.exec_blocking(['C:/Windows/System32/ipconfig', '/all'])
        if len(mt) > 6:
            tmp = [ln for ln in mt.split("\r\n") if "IPv4" in ln]
            return [
                ln.split(":")[1].replace("(Preferred)", "").strip() for ln in tmp]
        return "unknown"

# Map from uname string to HostInfo type
HOST_INFO_MAP = {'SunOS' : SolarisHostInfo,
                 'Darwin' : MacHostInfo,
                 'Windows' : WindowsHostInfo,
                 'CYGWIN' : CygwinHostInfo}

class ABClusterHost(object):
    """Base class providing common interface."""
    __meta_class__ = abc.ABCMeta

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
        """Abstract"""
        pass

    @abc.abstractmethod
    def get(self, host, filename, localname):
        """Abstract"""
        pass

    @abc.abstractmethod
    def put(self, host, localname, filename):
        """Abstract"""
        pass

    @property
    def hostInfo(self):
        """Create an appropriate HostInfo object for localhost. Uses Pythons platform.system() to
         determine the type of HostInfo object to return."""

        if self._hostInfo is None:
            (system, machine, osver, osflavor) = self._get_system_tuple()
            if HOST_INFO_MAP.has_key(system):
                self._hostInfo = HOST_INFO_MAP[system](self, system, machine, osver, osflavor)
            else:
                dbg_msg = 'CHS--> Using default HostInfo for host %s (%s, %s)'
                _logger.debug(dbg_msg, self.host, system, machine)
                return HostInfo(self, system, machine, osver, osflavor)

        return self._hostInfo

    @property
    def path_module(self):
        """Path module to use when manipulating file paths."""
        return self.hostInfo.path_module

    @property
    def env(self):
        """environment"""
        if not self._env:
            ctx = {'str': self.exec_blocking(self.hostInfo.envcmd), 'properties': {}}
            util.parse_properties(ctx)
            self._env = ctx['properties']

        return self._env

    @property
    def ram(self):
        """Returns total amount of RAM in MB"""
        return self.hostInfo.ram

    @property
    def cores(self):
        """Returns how many logical processors host has."""
        return self.hostInfo.cores

    @property
    def uname(self):
        """UNAME for host."""
        return self.hostInfo.uname

    @property
    def installdir(self):
        """Where to find MySQL executables"""
        return self.hostInfo.installdir

    @property
    def homedir(self):
        """HOME for logged in user"""
        return self.hostInfo.homedir

    @property
    def disk_free(self):
        """Free disk space on HOME partition or 'unknown'."""
        return self.hostInfo.disk_free

    @property
    def docker_info(self):
        """Return Docker service status on host; 'RUNNING', 'NOT RUNNING' or 'NOT INSTALLED'"""
        return self.hostInfo.docker_info

    @property
    def intip(self):
        """Depending on how many IP addresses various NET commands filtered out, we return single
         or array of IP addresses deemed as "Internal"""
        return self.hostInfo.intip

    @abc.abstractmethod
    def drop(self, paths=[]):
        """Close open connections and remove files.
        paths - list of files to remove from host before closing connection
        """
        map(self.rm_r, paths)

    @abc.abstractmethod
    def file_exists(self, path):
        """Test for the existence of a file. If the file actually exists, its stat object is
         returned, otherwise None.
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
        """Provides mkdir -p type functionality. I.e, missing parent directories are also created.
        If the directory we are trying to create already exists, we silently do nothing. If path or
         any of its parents is not a directory an exception is raised.
        path - directory to create on remote host """
        pass

    @abc.abstractmethod
    def rm_r(self, path):
        """Provides rm -r type functionality. All files and directories are removed recursively.
        path - file or directory to remove."""
        pass

    def auto_complete(self, basedir, locations, executable):
        """Find the absolute path of an executable given a prefix directory, a set of possible
         directories, and the basename of the executable.
        basedir - basedir of a cluster installation
        locations - list of directories to try
        executable - basename of executable to auto-complete"""
        for l in locations:
            choice = posixpath.join(basedir, l, executable)
            _logger.debug('Testing if %s exists.', choice)
            if self.file_exists(choice):
                _logger.debug('%s exists.', choice)
                return choice
            else:
                _logger.debug('%s doesn\'t exist.', choice)
        raise Exception('Cannot locate '+executable+' in '+
                        posixpath.join(basedir, str(locations)) + ' on host ' + self.host)

    @abc.abstractmethod
    def _exec_cmdv(self, cmdv, procCtrl, std_in_file):
        pass

    def _execfast(self, cmdv):
        pass

    def execfast(self, cmdv):
        """Forwards to virtual."""
        return self._execfast(cmdv)

    def exec_cmdv(self, cmdv, procCtrl={'waitForCompletion': True}, std_in_file=None):
        """Forwards to virtual."""
        return self._exec_cmdv(cmdv, procCtrl, std_in_file)

    def exec_blocking(self, cmdv):
        """Convenience method."""
        assert(isinstance(cmdv, list))
        return self.exec_cmdv(cmdv, {'waitForCompletion': True})

    def exec_cluster_daemon(self, cluster_daemon, waitsec):
        """Convenience method."""
        assert(isinstance(cluster_daemon, list))
        return self.exec_cmdv(cluster_daemon, {'daemonWait': waitsec})

class LocalClusterHost(ABClusterHost):
    """Implement the ABClusterHost interface for access to the localhost without using SSH over
     Paramiko. Note that this implies that there will be no authentication."""
    def __init__(self, host):
        super(type(self), self).__init__()
        self.host = host
        self.osver = None
        self.osflavor = None
        self.hlpstr = None
        self.system = None

    def _get_system_tuple(self):
        if self.system is not None:
            return (self.system, self.hlpstr, self.osver, self.osflavor)

        osver = None
        osflavor = None
        hlp_str = None
        system = platform.system()
        if system == 'Windows':
            try:
                subprocess.check_call(['uname'])
            except WindowsError:
                _logger.debug('CHS--> No uname available, assuming native Windows')
                # systeminfo slows things down significantly.
                m = platform.platform()
                #system = m.split("-")[0]
                osver = m.split("-")[1]
                osflavor = "Microsoft " + system + " " + osver
                self.osver = osver
                self.osflavor = osflavor
                self.hlpstr = platform.uname()[-2]
                self.system = system
                return (system, platform.uname()[-2], osver, osflavor)
            else:
                self.osver = osver
                self.osflavor = osflavor
                self.hlpstr = 'Unknown'
                self.system = 'CYGWIN'
                return ('CYGWIN', 'Unknown', osver, osflavor)
        #System is either Linux, SunOS or Darwin
        if system.startswith('Darwin'):
            osver = str(self.exec_blocking(['uname', '-r']))
            osflavor = "MacOSX"

        if system.startswith("SunOS"): #or system.startswith("Solaris"):
            osver = str(self.exec_blocking(['uname', '-v']))
            osflavor = "Solaris"

        if "Linux" in system:
            system = "Linux"
            # Assumption is all Linux flavors will have /etc/os-release file.
            if self.file_exists('/etc/os-release'):
                hlp_str = self.exec_blocking(['test', '-f', '/etc/os-release'])
            else:
                hlp_str = "0"
            if hlp_str != "0":
                hlp_str = self.exec_blocking(['cat', '/etc/os-release'])
                matched_lines = [line for line in hlp_str.split('\n') if "ID=" in line]
                hlp = (str(matched_lines[0]).split("ID=", 1)[1]).strip('"')
                osflavor = hlp
                matched_lines = [line for line in hlp_str.split('\n') if "VERSION_ID=" in line]
                hlp = (str(matched_lines[0]).split("VERSION_ID=", 1)[1]).strip('"')
                osver = hlp
            else:
                #Bail out, no file
                wrn_msg = 'CHS--> %s, %s, %s does not have /etc/os-release file!'
                _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, system, osflavor, osver)

        self.osver = osver
        self.osflavor = osflavor
        self.hlpstr = platform.uname()[-1]
        self.system = system
        return (system, platform.uname()[-1], osver, osflavor)

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
        system = platform.system()
        if system == 'Windows':
            # On Windows, there is no path auto-complete and the startup script (ndb_setup.py)
            # returns BASEDIR so we need to add \bin.
            pat = request_handler.BASEDIR
            if not "\bin" in pat or not "/bin" in pat:
                pat = os.path.join(pat, 'bin')
            return pat
        else:
            return request_handler.BASEDIR

    def open(self, filename, mode='r'):
        """Open a file on ABClusterHost."""
        return open(filename, mode)

    def get(self, filename, localname):
        """Get a file on ABClusterHost and save to ~/.mcc."""
        #File names are complete with path.
        return shutil.copy(filename, localname)

    def put(self, localname, filename):
        """Copy file."""
        #File names are complete with path.
        return shutil.copy(localname, filename)

    def drop(self, paths=[]):
        """Close open connections and remove files.
        paths - list of files to remove from host before closing connection
        """
        map(self.rm_r, paths)

    def file_exists(self, path):
        """Test for the existence of a file on the local host. If the file
        actually exists, its stat result object is returned, otherwise None.
        path - file to check the existence of
        """
        if os.path.exists(path):
            return os.stat(path)
        return None

    def list_dir(self, path):
        """List the files in a directory on the local host. Forwards to os.listdir().
        path - directory to list
        """
        return os.listdir(path)

    def mkdir_p(self, path):
        """Provides mkdir -p type functionality on localhost. Does nothing if
        the directory already exists, otherwise forwards to os.makedirs.
        path - directory to create on remote host
        """
        if os.path.exists(path) and os.path.isdir(path):
            return
        os.makedirs(path)

    def rm_r(self, path):
        """Provides rm -r type functionality on localhost. Forwards to os.rmdirs.
        path - file or directory to remove
        """
        shutil.rmtree(path)

    def _execfast(self, cmdv):
        """Quick and dirty exec for localhost. Not sure locking is even needed."""
        # global CMD_LOCK
        # with CMD_LOCK:
        proc = subprocess.Popen(cmdv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                shell=True)
        out = proc.communicate()[0]
        rc = proc.returncode
        if rc == 0:
            return out
        return 'errcode:'+str(rc)+'\n' + out

    def _exec_cmdv(self, cmdv, procCtrl, stdin_file):
        """Execute an OS command on the local host, using subprocess module.
        cmdv - complete command vector (argv) of the OS command
        procCtrl - procCtrl object from message which controls how the process
        is started (blocking vs non-blocking and output reporting)
        """
        # Add nohup when running locally to prevent server from waiting on the children
        if util.get_val(procCtrl, 'nohup'):
            cmdv[:0] = ['nohup']
        stdin = None
        if stdin_file != None:
            stdin = self.open(stdin_file)

        #We need Popen to have all possible FE commands to work. Using single mechanism to run all
        # commands adds to readability. I am hoping to reduce number of exec_cmd functions too :-/
        # Several possible cases:
        # 1) !waitForCompletion and !daemonWait: Just fire and forget.
        # 2) waitForCompletion and !daemonWait: Wait for result.
        # 3) waitForCompletion and daemonWait: Wait for result but poll after daemonWait seconds.
        # 4) !waitForCompletion and daemonWait: Wait for result only daemonWait seconds.
        # Old code was partial 2) and 3)+4).

        #New code:
        # Wait for daemonWait AND MAX 120 seconds for command to return but check if it finished
        # every second (after waiting daemonWait).

        #Rationale: We have commands that can not return immediately even on localhost. For such
        # commands we set daemonWait member of procControl to appropriate amount of seconds (say 2)
        # forcing this exec routine to always wait that long for response. After that, we allow
        # maximum of 120s for command to complete before we kill it. If command does not have
        # procControl.daemonWait this means we expect entire process to finish as soon as command
        # returns. For such commands, we wait max 120s.
        proc_DW_tot = 120
        proc_DW = 0
        if procCtrl.has_key('daemonWait'):
            proc_DW = util.get_val(procCtrl, 'daemonWait')
        tst = ""
        if isinstance(cmdv, list):
            tst = ' '.join(cmdv)
        else:
            tst = str(cmdv)
        _logger.debug("CHS--> daemonWait is %s", proc_DW)
        _logger.debug("CHS--> cmdv %s", tst)
        # hasKill = procCtrl.has_key('kill'). Commands with Kill member have different processing.
        kill_it = True
        try:
            if isinstance(cmdv, list):
                proc = subprocess.Popen(cmdv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            else:
                proc = subprocess.Popen(shlex.split(cmdv), stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT)
            # There are commands we have to wait for to start with, such as NET START/STOP etc.
            _logger.debug("CHS--> proc %s executed", tst)
            if proc_DW > 0:
                _logger.debug("CHS--> proc %s executed, sleeping", tst)
                time.sleep(proc_DW)
            #We have waited set amount of time, now see if it returns in 2min and if not, kill it.
            for t in xrange(proc_DW_tot):
                time.sleep(0.5)
                if proc.poll() is not None:
                    kill_it = False
                    _logger.debug("CHS--> %ss, breaking for %s", t/2, tst)
                    if proc_DW > 0:
                        dbg_msg = "CHS--> Waited %ssec (on top of %ssec) for %s to complete."
                        _logger.debug(dbg_msg, t/2, proc_DW, tst)
                    else:
                        _logger.debug("CHS--> Waited %s seconds for %s to complete.", t/2, tst)
                    break

            if kill_it:
                _logger.warning(bcolors.WARNING + "CHS--> killing %s" + bcolors.ENDC, tst)
                proc.kill()
                retcode = proc.returncode
                if retcode is None:
                    retcode = 1
                rd = proc.stdout.read()
                wrn_msg = "CHS--> killing, raising %s rc %s, msg %s"
                _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, tst, retcode, rd)
                raise ExecException(tst, retcode, proc.stdout)
            else:
                retcode = proc.returncode
                rd = proc.stdout.read()
                if retcode is None: #REALLY BAD except if it's meant to be so, such as with KILL -9
                    retcode = retcode or -1
                    rd = rd or "No output from command."
                    wrn_msg = "CHS--> killing, raising %s rc %s, msg %s"
                    _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, tst, retcode, rd)
                    raise ExecException(tst, proc.returncode, proc.stdout)
                elif retcode == 0  or (retcode != 0 and \
                    retcode == util.get_val(procCtrl, 'noRaise')):
                    _logger.debug("CHS--> Conditional succ. noRise:%s.", retcode or -1)
                    # When successful, cut it to command and 100 characters of response. To remove
                    # just empty lines and not lines with spaces:
                    #"".join([s for s in t.strip().splitlines(True) if s.strip("\r\n")])
                    rd = rd or "No output from command." # rd can be None, for example for KILL -9
                    _logger.debug("CHS--> Succ. %s(%s), %s...", tst, retcode, \
                        "".join([t for t in rd.strip().splitlines(True) if t.strip()])[0:100])
                    return rd
                elif retcode <= 125:
                    wrn_msg = "CHS--> %s failed, exit-code=%d error = %s"
                    _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, tst, retcode, rd)
                    return 'errcode:'+str(retcode)+'\n' + rd
                elif retcode == 127:
                    wrn_msg = bcolors.WARNING + "CHS--> %s, program not found: %s" + bcolors.ENDC
                    _logger.warning(wrn_msg, tst, rd)
                    return 'errcode:'+str(retcode)+'\n' + rd
                else:
                    # Things get hairy and unportable - different shells return
                    # different values for coredumps, signals, etc.
                    rd = rd or "No output from command." # rd can be None
                    _logger.warning(bcolors.WARNING + "CHS--> %s, program reported OS-dependant "
                                    "exit-code=%d error = %s" + bcolors.ENDC, tst, retcode, rd)
                    return 'errcode:'+str(retcode)+'\n' + rd
        finally:
            if stdin is not None:
                stdin.close()

    def execute_command(self, cmdv, inFile=None):
        """Execute an OS command blocking on the local host, using subprocess module. Returns dict
        containing output from process.
        cmdv - complete command vector (argv) of the OS command."""
        out_file = tempfile.TemporaryFile()
        err_file = tempfile.TemporaryFile()
        # This will fail if command needs shell-expansion. Also, no timeout.
        result = {
            'exitstatus': subprocess.call(args=cmdv, stdin=inFile, stdout=out_file, stderr=err_file)
            }
        out_file.seek(0)
        err_file.seek(0)
        result['out'] = out_file.read()
        result['err'] = err_file.read()
        _logger.debug('CHS--> execute_command %s on localhost, result %s', ' '.join(cmdv), result)
        return result

def produce_ABClusterHost(hostname='localhost', key_based=None, user=None, pwd=None, key_file=None):
    """Factory method which returns RemoteClusterHost or LocalClusterHost depending on the value of
     hostname.."""
    _logger.debug('CHS--> Produce ABC, hostname passed %s', hostname)
    shn = socket.gethostbyname_ex(socket.gethostname())
    #Say ('host.dom.comp.com', [], ['10.17.16.24'])
    _logger.debug('CHS--> Produce ABC socket host name is %s.', shn)
    if hostname == 'localhost' or hostname == '127.0.0.1' or hostname == shn[0]:
        _logger.debug('CHS--> Host is local.')
        return LocalClusterHost(hostname)

    for x in range(len(shn[2])):
        if hostname == shn[2][x]:
            return LocalClusterHost(hostname)

    # Sanitize input:
    if not (user and user.strip()):
        user = None
    if not (pwd and pwd.strip()):
        pwd = None
    else:
        if pwd.startswith('**'):
            pwd = None
    if not (key_file and key_file.strip()):
        key_file = None
    import remote_clusterhost
    _logger.debug('CHS--> Producing remote ABC.')
    return remote_clusterhost.RemoteClusterHost(hostname, key_based, user, pwd, key_file)
