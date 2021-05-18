# Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
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

#!/bin/env python
import platform

(pymajor, pyminor, pypatch) = map(int, platform.python_version_tuple())
assert pymajor == 2 

import sys

import os
import os.path

tst_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
mcc_dir = os.path.dirname(tst_dir)

# Paramiko from Bazaar 
sys.path += [tst_dir, mcc_dir, '/opt/csw/lib/python/site-packages' ]

import logging
if pyminor < 7:
    print('Running with Python version %s', 
          str(platform.python_version_tuple()))
    import unittest2 as unittest
else:
    import unittest

utmod = unittest

import traceback    
import json
import urlparse
import time
import socket
import stat
import tempfile
import platform
import time
import contextlib

import request_handler
import config_parser
import util

from util import mock_msg, is_set
from clusterhost import ABClusterHost, LocalClusterHost, produce_ABClusterHost
from remote_clusterhost import RemoteClusterHost

from paramiko import SSHClient, WarningPolicy

_ndb_config = os.path.join('..', '..', '..', 'bin', 'ndb_config')

def tst_tempify(*fs):
	td = util.get_val(os.environ, 'MYSQL_TMP_DIR', tempfile.gettempdir())
	#td = tempfile.mkdtemp()
	return os.path.join(td, *fs)

def defdatadir():
    return os.path.join(os.path.expanduser('~'), 'MySQL_Cluster', 'data')

# Default tst configuration. Can be overridden in config.json
_cfg = { 'debuglevel': 'DEBUG' }

def cfg():
    return _cfg

request_handler.configdir = '..'

def_datadir = os.path.join(os.path.expanduser('~'), 'MySQL_Cluster', 'data')


def mock_ABClusterHost(hle):
    return produce_ABClusterHost(hostname=hle['hostInfoRep']['host']['name'], user=util.get_val(hle, 'username'), pwd=util.get_val(hle, 'password'))

def local_ipv4_ssh_addr():
	for (family, socktype, proto, canonname, sockaddr) in socket.getaddrinfo(socket.gethostname(), 22):
		if family == socket.AF_INET:
			return sockaddr[0]
		
def host_is_unreachable(hostname, port=22):
    rd = util._retdict('ECONNREFUSED', True)
    rd[socket.EAI_NONAME] = True

    return util.try_connect(hostname, port, False, rd)

def is_local_sshd_available():
    with contextlib.closing(SSHClient()) as c:
        c.set_missing_host_key_policy(WarningPolicy())
        #c.load_system_host_keys()
        try:
            c.connect(local_ipv4_ssh_addr(), password=util.get_val(os.environ, 'SSH_PWD'))
        except:
            logging.exception('No usable sshd on this machine: ')
            return False
        else:
            return True

def mock_msg_as_json(cmd, body):
    return json.dumps(mock_msg(cmd,body))

def json_normalize(x):
    return json.loads(json.dumps(x))

class Test00Utils(utmod.TestCase):
    def test_version_tuple(self):
        print [ int(filter(str.isdigit, vn)) for vn in  ('2', '7', '2+')]

    def test_Param(self):
        print util.Param({'name':'--c', 'val':'bar', 'sep':'@'})
        
        x = util.Param('--a')
        print json.dumps(x)
        print util.Param('--b','foo')
        print util.Param('--d','baz','#')

    def test_host_is_unreachable(self):
        self.assertTrue(host_is_unreachable('some_non_existent_host_name'))

    @utmod.skipIf(not is_local_sshd_available(), 'FIXME: Sshd is not running by default on Windows - test some other port?')
    def test_host_is_unreachable(self):
        self.assertFalse(host_is_unreachable(local_ipv4_ssh_addr()))

    def test_to_json(self):
        obj = [ "sleep" "300" ]
        json_string = json.dumps(obj)
        jobj = json.loads(json_string)
        self.assertEqual(obj, jobj)

class Test0ConfigIni(utmod.TestCase):
    def setUp(self):
        self.ini = os.path.join(tst_dir, 'example_config.ini')
        self.cp = config_parser.parse_config_ini(self.ini)
    
    def tearDown(self):
        pass
            
    def test_get_option_values(self):
        self.assertEqual(config_parser.get_option_value_set(self.cp, 'HostName'), set(['siv27','siv28']))
 
    def test_get_node_dicts(self):
        print config_parser.get_node_dicts(self.cp, 0)
        
    def test_get_configvalues(self):
        print config_parser.get_configvalues(self.cp)
        print config_parser.get_processes(self.cp)

    def test_parse(self):
        logging.debug('ex_ini as json:\n'+json.dumps(config_parser.parse_cluster_config_ini(self.ini)))

    def test_parsex(self):
        logging.debug('ex_ini as json:\n'+str(config_parser.parse_cluster_config_ini_x(self.ini)))

    def test_write(self):
        logging.debug('ex_ini write back:\n'+config_parser.write_cluster_config_ini(config_parser.parse_cluster_config_ini(self.ini)))

    @utmod.skip('Need a way to specify the cluster installdir (default cluster we are part of?')
    def test_ndb_config(self):
        print 'ndb config as json: ', json.dumps(util.xml_to_python(self.cluster.ndb_config_xml())) 
        
@utmod.skipIf(not is_local_sshd_available(), 'FIXME: Sshd is not running by default on Windows - test some other port?')
class Test3Ports(utmod.TestCase):
    def testPortNotAvailable(self):
        self.assertFalse(util.is_port_available(local_ipv4_ssh_addr(), 22))

    def testPortAvailable(self):
        self.assertTrue(util.is_port_available(local_ipv4_ssh_addr(), 23))

    def testFindPort(self):
        self.assertEqual(util.first_available_port(local_ipv4_ssh_addr(), 22, 23), 23)

    def testNotFindPort(self):
        self.assertRaises(util.NoPortAvailableException, 
                          util.first_available_port, local_ipv4_ssh_addr(), 22, 22)
       
class _TestABClusterHost:
    def setUp(self):
        self.rtd = time.strftime("%Y-%m-%dT%H-%M-%S",time.localtime())+str(self.__class__.__name__)
        logging.debug('rtd: '+self.rtd)
        self.ch.mkdir_p(self.rtd)

    def tearDown(self):
        if sys.exc_info() == (None, None, None):
            logging.debug('tearDown: success')
            self.ch.rm_r(self.rtd)
        else:
            logging.debug('tearDown: failure')
            logging.exception('try this...')
        self.ch.drop()
        
    @utmod.skip('FIXME: Need the path to a program that will always be available')
    def test_fg_exec(self):
        self.assertTrue('This program will retrieve config options for a ndb cluster' in self.ch.exec_blocking([_ndb_config, '--help']))
            
    def test_fileops(self):
        d = self.ch.path_module.join(self.rtd, 'foo', 'bar', '')
        logging.debug('d='+d)
        self.ch.mkdir_p(d)
        bazname = self.ch.path_module.join(d, 'baz')
        with self.ch.open(bazname, 'w+') as f:
            f.write('Some text here...\n')

        with self.ch.open(bazname) as f:
            self.assertEqual(f.read(), 'Some text here...\n')

    def test_stat_dir_2(self):
        if not hasattr(self.ch, 'sftp'):
            return
        self.assertTrue(stat.S_ISDIR(self.ch.sftp.stat(self.rtd).st_mode))

    @utmod.skip('Cannot create files with attributes')    
    def test_list_dir_no_x(self):
        nox_name = self.ch.path_module.join(self.rtd, 'nox')
        notmine_name = self.ch.path_module.join(self.rtd, 'not_mine')
        nox_mode = self.self.ch.sftp.stat(nox_name).st_mode
        notmine_mode = self.self.ch.sftp.stat(notmine_name).st_mode

        self.assertTrue(stat.S_ISDIR(nox_mode))
        self.assertTrue(is_set(stat.S_IMODE(nox_mode), stat.S_IXGRP))
        
        self.assertTrue(stat.S_ISDIR(notmine_mode))
        self.assertTrue(is_set(stat.S_IMODE(notmine_mode), stat.S_IXOTH))

        logging.debug('listdir(nox)='+str(self.self.ch.list_dir(nox_name)))
        logging.debug('listdir(notmine)='+str(self.self.ch.list_dir(notmine_name)))
        
    def test_mkdir_p(self):
        somedir_name = self.ch.path_module.join(self.rtd, 'some_dir')
        logging.debug('somedir_name'+somedir_name)
        self.ch.mkdir_p(somedir_name)

    def test_hostInfo(self):
        self.assertGreater(self.ch.hostInfo.ram, 1000)
        self.assertGreater(self.ch.hostInfo.cores, 0)
        self.assertIsNotNone(self.ch.hostInfo.homedir)


class Test4LocalClusterHost(utmod.TestCase, _TestABClusterHost):
    def setUp(self):
        self.ch = LocalClusterHost('localhost')
        _TestABClusterHost.setUp(self)

    def tearDown(self):
        _TestABClusterHost.tearDown(self)

@utmod.skipIf(not is_local_sshd_available(), 'No suitable local sshd')
class Test5RemoteClusterHost(utmod.TestCase, _TestABClusterHost):
    def setUp(self):
        self.ch = RemoteClusterHost(local_ipv4_ssh_addr(), password=util.get_val(os.environ, 'SSH_PWD'))
        _TestABClusterHost.setUp(self)
        
    def tearDown(self):
        _TestABClusterHost.tearDown(self)

    def test_copy_file(self):
        if not hasattr(self.ch, 'sftp'):
            return

        content = 'Some text here\nSome more text on another line\n'
        (lh, lname) = tempfile.mkstemp()
        logging.debug('lname: '+lname)
        try:
            os.write(lh, content)
			
            # Note! Might not work with a genuine remote host, as rex.name
            # might not be creatable there
            (rh, rname) = tempfile.mkstemp()
            os.close(rh)
            # Fixme! Assumes SFTP home is C: (More commonly HOMEPATH or USERPROFILE? 
            rname = os.path.basename(rname)
            self.ch.sftp.put(lname, rname)
            try:
                (llh, llname) = tempfile.mkstemp()
                os.close(llh)
                self.ch.sftp.get(rname, llname)
                try:
                    with open(llname) as llh:
                        self.assertEqual(content, llh.read())
                finally:
                    os.remove(llname)
            finally:
                self.ch.sftp.remove(rname)
        finally:
            os.close(lh)
            os.remove(lname)
            
    def test_mkdir(self):
        if not hasattr(self.ch, 'sftp'):
            return
        (h, n) = tempfile.mkstemp()
        os.write(h, "Some text here\nSome more text on another line\n")
        os.close(h)

        some_new_dir_name = self.ch.path_module.join(self.rtd, 'some_new_dir')
        self.ch.sftp.mkdir(some_new_dir_name)
        tmpex_name = self.ch.path_module.join(some_new_dir_name, 'example.txt')
        self.ch.sftp.put(n, tmpex_name)
        os.remove(n)

    def test_stat_dir(self):
        if not hasattr(self.ch, 'sftp'):
            return
        self.assertEqual(stat.S_IFMT(self.ch.sftp.stat(self.rtd).st_mode), stat.S_IFDIR)


class Test6RequestHandler(utmod.TestCase):
    def setUp(self):
        self.ssh = {'user': None, 'pwd': None }
        
    def tearDown(self):
        pass


    def test_hostInfoReq(self):
        json_str = mock_msg_as_json('hostInfoReq', {'ssh': self.ssh,
                                    'hostName': 'localhost'})
        print 'hostInfoReq: '+json_str
        print request_handler.handle_req(json.loads(json_str))

    def test_createFileReq(self):
        json_str = mock_msg_as_json('createFileReq', 
          {'ssh': self.ssh, 'file': {'hostName': 'localhost', 'path': tempfile.gettempdir(), 'name': 'foobar'}, 
           'contentString': 'a=0\nb=1\n', 'contents': {'params': {'sep': None, 'para': [{'name': None, 'sep': None, 'val': None}]}}})
        print json_str
	try:
          print request_handler.handle_req(json.loads(json_str))
	finally:
          try:
            os.remove(os.path.join(tempfile.gettempdir(),'foobar'))
          except:
            pass
        
if __name__ == '__main__':
    if cfg()['debuglevel'] is not None:
        import logging
        fmt = '%(asctime)s: %(levelname)s [%(funcName)s;%(filename)s:%(lineno)d]: %(message)s '
        logging.basicConfig(level=getattr(logging, cfg()['debuglevel']), format=fmt)
   
    try:
        import xmlrunner2
    except:
        traceback.print_exc()
        assert(False) 
        utmod.main(argv=sys.argv)
    else:
        if os.environ.has_key('XMLRUNNER'):
            utmod.main(argv=sys.argv, testRunner=xmlrunner2.XMLTestRunner)
        else:
            utmod.main(argv=sys.argv)
