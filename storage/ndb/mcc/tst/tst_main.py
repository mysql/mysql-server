# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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

#!/bin/env python
import platform

(pymajor, pyminor, pypatch) = map(int, platform.python_version_tuple())
assert pymajor == 2 

import sys

# Paramiko from Bazaar 
sys.path += ['..', '/usr/local/lib/bzr-2.1.0-2010.03/lib/python2.6/site-packages', '/usr/local/lib/bzr-2.1.0/lib/python/site-packages', '/opt/csw/lib/python/site-packages' ]

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
import os.path
import tempfile
import os
import platform


import request_handler
import config_parser
#import clumodel
import util

from util import mock_msg, is_set
from clusterhost import ABClusterHost, LocalClusterHost, produce_ABClusterHost
from remote_clusterhost import RemoteClusterHost

from paramiko import SSHClient, WarningPolicy

_ndb_config = os.path.join('..', '..', '..', 'bin', 'ndb_config')

def tst_tempify(*fs):
    return os.path.join(util.get_val(os.environ, 'MYSQL_TMP_DIR', 
                        tempfile.gettempdir()), *fs)

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

def host_is_unreachable(hostname, port=22):
    rd = util._retdict('ECONNREFUSED', True)
    rd[socket.EAI_NONAME] = True

    return util.try_connect(hostname, port, False, rd)

def is_local_sshd_available():
    c = SSHClient()
    c.set_missing_host_key_policy(WarningPolicy())
    c.load_system_host_keys()
    try:
        c.connect('localhost', password=util.get_val(os.environ, 'SSH_PWD'))
    except:
        logging.exception('No usable sshd on this machine: ')
        return False
    else:
        return True
    finally:
        c.close()

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
        self.assertFalse(host_is_unreachable('localhost'))

    def test_to_json(self):
        obj = [ "sleep" "300" ]
        json_string = json.dumps(obj)
        jobj = json.loads(json_string)
        self.assertEqual(obj, jobj)

class Test0ConfigIni(utmod.TestCase):
    def setUp(self):
        self.cp = config_parser.parse_config_ini('example_config.ini')
    
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
        logging.debug('ex_ini as json:\n'+json.dumps(config_parser.parse_cluster_config_ini('example_config.ini')))

    def test_parsex(self):
        logging.debug('ex_ini as json:\n'+str(config_parser.parse_cluster_config_ini_x('example_config.ini')))

    def test_write(self):
        logging.debug('ex_ini write back:\n'+config_parser.write_cluster_config_ini(config_parser.parse_cluster_config_ini('example_config.ini')))

    @utmod.skip('Need a way to specify the cluster installdir (default cluster we are part of?')
    def test_ndb_config(self):
        print 'ndb config as json: ', json.dumps(util.xml_to_python(self.cluster.ndb_config_xml())) 
        
@utmod.skipIf(not is_local_sshd_available(), 'FIXME: Sshd is not running by default on Windows - test some other port?')
class Test3Ports(utmod.TestCase):
    def testPortNotAvailable(self):
        self.assertFalse(util.is_port_available('localhost', 22))

    def testPortAvailable(self):
        self.assertTrue(util.is_port_available('localhost', 23))

    def testFindPort(self):
        self.assertEqual(util.first_available_port('localhost', 22, 23), 23)

    def testNotFindPort(self):
        self.assertRaises(util.NoPortAvailableException, 
                          util.first_available_port, 'localhost', 22, 22)
       
class _TestABClusterHost:
    def setUp(self):
        pass

    def tearDown(self):
        self.ch.drop()
    @utmod.skip('FIXME: Need the path to a program that will always be available')
    def test_fg_exec(self):
        self.assertTrue('This program will retrieve config options for a ndb cluster' in self.ch.exec_blocking([_ndb_config, '--help']))
            
    def test_fileops(self):
        d = tst_tempify('foo', 'bar', '')
        logging.debug('d='+d)
        self.ch.mkdir_p(d)
        f = None
        try:
            f = self.ch.open(tst_tempify('foo', 'bar', 'baz'), 'w+')
            f.write('Some text here...\n')
            f.seek(0)
            self.assertEqual(f.read(), 'Some text here...\n')
        finally:
            if f is not None:
                f.close()
            self.ch.rm_r(tst_tempify('foo'))

    def test_stat_dir_2(self):
        if not hasattr(self.ch, 'sftp'):
            return
        self.assertTrue(stat.S_ISDIR(self.ch.sftp.stat(tst_tempify('')).st_mode))

    @utmod.skip('Cannot create files with attributes')    
    def test_list_dir_no_x(self):
        nox_name = tst_tempify('nox')
        notmine_name = tst_tempify('not_mine')
        nox_mode = self.self.ch.sftp.stat(nox_name).st_mode
        notmine_mode = self.self.ch.sftp.stat(notmine_name).st_mode

        self.assertTrue(stat.S_ISDIR(nox_mode))
        self.assertTrue(is_set(stat.S_IMODE(nox_mode), stat.S_IXGRP))
        
        self.assertTrue(stat.S_ISDIR(notmine_mode))
        self.assertTrue(is_set(stat.S_IMODE(notmine_mode), stat.S_IXOTH))

        print "listdir(nox)="+str(self.self.ch.list_dir(nox_name))
        print "listdir(notmine)="+str(self.self.ch.list_dir(notmine_name))        

    def test_mkdir_p(self):
        somedir_name = tst_tempify('some_dir')
        logging.debug('somedir_name'+somedir_name)
        self.ch.mkdir_p(somedir_name)
        self.ch.rm_r(somedir_name)

    def test_hostInfo(self):
        hir = json_normalize(self.ch.hostInfo.rep)
        self.assertGreater(hir['hostRes']['ram'], 1000)
        self.assertGreater(hir['hostRes']['cores'], 0)


class Test4LocalClusterHost(utmod.TestCase, _TestABClusterHost):
    def setUp(self):
        _TestABClusterHost.setUp(self)
        self.ch = LocalClusterHost('localhost')

    def tearDown(self):
        _TestABClusterHost.tearDown(self)

@utmod.skipIf(not is_local_sshd_available(), 'No suitable local sshd')
class Test5RemoteClusterHost(utmod.TestCase, _TestABClusterHost):
    def setUp(self):
        _TestABClusterHost.setUp(self)
        self.ch = RemoteClusterHost(socket.gethostname(), password=util.get_val(os.environ, 'SSH_PWD'))

    def tearDown(self):
        _TestABClusterHost.tearDown(self)

    def test_copy_file(self):
        with tempfile.NamedTemporaryFile() as ex:
            ex.write("Some text here\nSome more text on another line\n")
            
            if not hasattr(self.ch, 'sftp'):
                return
            ex.seek(0)
            tmpex_name = tst_tempify('example.txt')
            self.ch.sftp.put(ex.name, tmpex_name)
            try:
                self.ch.sftp.get(tmpex_name, 'remote_example.txt')
                with open('remote_example.txt') as rex:
                    self.assertEqual(ex.read(), rex.read())
            finally:
                self.ch.sftp.remove(tmpex_name)

    def test_mkdir(self):
        with tempfile.NamedTemporaryFile() as ex:
            ex.write("Some text here\nSome more text on another line\n")
            ex.seek(0)
            if not hasattr(self.ch, 'sftp'):
                return
            some_new_dir_name = tst_tempify('some_new_dir')
            self.ch.sftp.mkdir(some_new_dir_name)
            tmpex_name = os.path.join(some_new_dir_name, 'example.txt')
            try:
                self.ch.sftp.put(ex.name, tmpex_name)
                self.ch.sftp.remove(tmpex_name)
            finally:
                self.ch.sftp.rmdir(some_new_dir_name)

    def test_stat_dir(self):
        if not hasattr(self.ch, 'sftp'):
            return
        self.assertEqual(stat.S_IFMT(self.ch.sftp.stat(tst_tempify('')).st_mode), stat.S_IFDIR)


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
        

    @utmod.skip('Too heavy - need to mock the execution')      
    def test_startClusterReq(self):
        json_str = mock_msg_as_json('startClusterReq', 
            {'ssh': self.ssh,
             'procs': [
                       {'file': {'autoComplete': True, 'hostName': 'localhost', 'path': cfg()['local_installdir'], 'name': 'ndb_mgmd'}, 
                                                                        'procCtrl': {'hup':True, 'getStd': False},
                                                                        'params': { 'param':  [ util.Param({'name':'Dummy1', 'sep':'=', 'val':42}) ], 'sep': ' ' }}]})
        print json_str
        print request_handler.handle_req(json.loads(json_str))
    


class Test7ConfiguratorServer(utmod.TestCase):
    def setUp(self):
        # TODO Start server
        #self.cluster = clumodel.create_cluster_from_config_ini('example_config.ini')
        pass

    def tearDown(self):
        
        #self.cluster.drop()
        #time.sleep(5.0) # To allow the cluster to really disappear
        # TODO Shutdown server
        pass
 
    @utmod.skip('Deprecated')
    def test_deploy_config(self):
        map(request_handler.handle_req, self.cluster.get_deploy_config_msgs())

    @utmod.skip('Deprecated')            
    def test_start_mgmds(self):
        self.cluster.deploy_config('example_config.ini', '/example_config.ini')
        self.cluster.start_nodes(clumodel.MgmdNode)
        time.sleep(1.0) # So the mgmds know about each other before calling tearDown

    @utmod.skip('Deprecated')  
    def test_start_ndbds(self):
        self.cluster.deploy_config('example_config.ini', '/example_config.ini')
        self.cluster.start_nodes(clumodel.MgmdNode)
        self.cluster.start_nodes(clumodel.NdbdNode)
        #time.sleep(2.0)
        
    @utmod.skip('Deprecated')  
    def test_start_mysqlds(self):
        self.cluster.deploy_config('example_config.ini', '/example_config.ini')
        self.cluster.start_nodes(clumodel.MgmdNode)
        self.cluster.start_nodes(clumodel.NdbdNode)
        time.sleep(30.0)
        self.cluster.start_nodes(clumodel.MysqldNode)
        time.sleep(30.0)

    @utmod.skip('For now')
    def test_start_cluster(self):
        map(request_handler.handle_req, self.cluster.get_deploy_config_msgs())
        request_handler.handle_req(self.cluster.get_start_msg())
        
        # Just so that tearDown actually will clean things up
        for n in self.cluster.nodes:
            n.is_started = True
    

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
        utmod.main(argv=sys.argv, testRunner=xmlrunner2.XMLTestRunner)
