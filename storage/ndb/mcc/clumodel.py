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

""" Provides classes that model a cluster and its different node types.  """
import time

import config_parser
from clusterhost import produce_ABClusterHost
from util import Param, mock_msg

class ClusterNode(object):
    def __init__(self, ndict, cluster):
        self.is_started = False
        self.nodeid = int(ndict['NodeId'])
        self.name = ndict['_NodeType']
        self.clusterhost = filter(lambda ch: ch.host == ndict['HostName'], cluster.hosts)[0]
        self.cluster = cluster
        self.nodedir = self.clusterhost.path_module.join(cluster.dstdir, repr(self.nodeid))
        self.datadir = self.clusterhost.path_module.join(self.nodedir, 'data')
        self.pidfile = self.clusterhost.path_module.join(self.datadir, 'ndb_'+repr(self.nodeid)+'.pid')
        self.clusterlog = self.clusterhost.path_module.join(self.datadir,  'ndb_' + repr(self.nodeid) + '_cluster.log')
        self.outlog = self.clusterhost.path_module.join(self.datadir, 'ndb_' + repr(self.nodeid) + '_out.log')
        self.clusterhost.mkdir_p(self.nodedir)
        self.clusterhost.mkdir_p(self.datadir)
        print 'Created nodedir=', self.nodedir, ' and datadir=', self.datadir, ' @' +self.clusterhost.host

    def get_procs(self):
        return [{'file': { 'autoComplete': True, 'hostName': self.clusterhost.host, 'path':self.cluster.basedir, 'name':self.name },
                'params': { 'param': self.get_param(), 'sep': ' ' },
                'procCtrl': {'hup': False, 'getStd': False, 'waitForCompletion': False }}]

    def start(self):
        try:
            self.start()
            self.is_started = True
        except:
            self.get_log()
            raise
        
    def get_log(self):
        if self.clusterhost.file_exists(self.clusterlog):
            lf = self.clusterhost.sftp.open(self.clusterlog)    
            print 'Dumping ' + self.clusterlog + ':\n' + lf.read()
            lf.close()
        
    def drop(self):
        self.clusterhost.sftp.stat(self.nodedir)
        self.clusterhost.rm_r(self.nodedir)
        
class MgmdNode(ClusterNode):
    def __init__(self, ndict, cluster):
        super(MgmdNode,self).__init__(ndict, cluster)
        self.port = ndict['PortNumber']
        
    def get_param(self):
        return [ Param('--no-defaults'), Param('--ndb-nodeid', self.nodeid),
                Param('--config-dir', self.datadir), Param('--config-file', self.clusterhost.path_module.join(self.datadir, 'config.ini')) ]
        
    def deploy_config(self, src, dst):
        self.clusterhost.sftp.put(src, self.cluster.dstdir+dst)
        
    def stop_cluster(self):
        print 'Stopping cluster through mgmd on {n.clusterhost.host}:{n.port}'.format(n=self)

#        sock = socket.create_connection((self.clusterhost.host, self.port))
#        sock.sendall('stop all\nabort: 0\nstop: db,mgm\n\n')
#        print("res: "+sock.recv(1024))
#        sock.shutdown(socket.SHUT_RDWR)
#        sock.close()
        mgm = self.clusterhost.auto_complete(self.cluster.basedir, ['bin'], 'ndb_mgm')
        print self.clusterhost.exec_blocking([mgm, '-e', 'SHUTDOWN', '{node.clusterhost.host}'.format(node=self), '{node.port}'.format(node=self)])

    def start(self):
        mgmd = self.clusterhost.auto_complete(self.cluster.basedir, ['bin'], 'ndb_mgmd')
        self.clusterhost.exec_blocking([mgmd, '--no-defaults', '--ndb-nodeid={node.nodeid}'.format(node=self), '--config-dir={node.cluster.dstdir}'.format(node=self), '--config-file={node.cluster.dstdir}/example_config.ini'.format(node=self)])
  


class NdbdNode(ClusterNode):
    def get_param(self):
        return [ Param('--ndb-nodeid', self.nodeid), Param('-c', 'siv27:41508,siv28:41512', ' ') ]
    
    def start(self):
        ndbd = self.clusterhost.auto_complete(self.cluster.basedir, ['bin'], 'ndbd')
        print self.clusterhost.exec_blocking([ndbd, ' --ndb-nodeid={node.nodeid}'.format(node=self), '-c', 'siv27:41508,siv28:41512'])

    
class MysqldNode(ClusterNode):
    def __init__(self, ndict, cluster):
        super(MysqldNode,self).__init__(ndict, cluster)
        self.port = ndict['_MysqlPort']
        self.pidfile = self.clusterhost.path_module.join(self.datadir, self.clusterhost.host+'.pid')

    def get_procs(self):
        return [
                {'file': { 'autoComplete': True, 'hostName': self.clusterhost.host, 'path':self.cluster.basedir, 'name': 'mysql_install_db' },
                'params': { 'param': [
                                      Param('--no-defaults'), Param('--ignore-builtin-innodb'), 
                                      Param('--datadir', self.datadir), Param('--basedir',self.cluster.basedir), Param('--port', self.port) ], 
                           'sep': ' ' },
                'procCtrl': {'hup': False, 'getStd': False, 'waitForCompletion': True, 'delay': 0 }},
                {'file': { 'autoComplete': True, 'hostName': self.clusterhost.host, 'path':self.cluster.basedir, 'name':self.name },
                'params': { 'param': 
                           [ Param('--no-defaults'), Param('--ignore-builtin-innodb'), Param('--ndbcluster','ON'), Param('--datadir', self.datadir), 
                            Param('--basedir',self.cluster.basedir), Param('--port', self.port), Param('--log-error', self.clusterlog), 
                            Param('--ndb-nodeid', self.nodeid), Param('--ndb-connectstring', 'siv27:41508,siv28:41512') ], 
                           'sep': ' ' },
                'procCtrl': {'hup': False, 'getStd': False, 'waitForCompletion': False,  'daemonWait': 5.0 }}                
                ]
                
    def start(self):
        install_db = self.clusterhost.auto_complete(self.cluster.basedir, ['scripts'], 'mysql_install_db')
        print self.clusterhost.exec_blocking([install_db, '--no-defaults', '--ignore-builtin-innodb', '--datadir={_.datadir}'.format(_=self), '--basedir={_.cluster.basedir}'.format(_=self), '--port={_.port}'.format(_=self)])
        
        print 'Starting mysqld...' 
        mysqld = self.clusterhost.auto_complete(self.cluster.basedir, ['bin'], 'mysqld')
        self.clusterhost.exec_cluster_daemon([mysqld, ' --no-defaults', '--ignore-builtin-innodb', '--ndbcluster=ON', '--datadir={_.datadir}'.format(_=self), '--basedir={_.cluster.basedir}'.format(_=self), '--port={_.port}'.format(_=self), '--log-error={_.clusterlog}'.format(_=self), '--ndb-nodeid={_.nodeid}'.format(_=self), '--ndb-connectstring=siv27:41508,siv28:41512'], 5.0)
        for x in range(0,10):
            if self.clusterhost.file_exists(self.clusterlog):
                break
            time.sleep(1.0)
        assert self.clusterhost.file_exists(self.clusterlog)
        print self.get_log()
              
    def stop(self):
        if self.is_started:
            mysqladmin = self.clusterhost.auto_complete(self.cluster.basedir, ['bin'], 'mysqladmin')
            print self.clusterhost.exec_blocking([mysqladmin, '-u', 'root', 'shutdown'])

def node_factory(ndict, c):
    nt = ndict['_NodeType']
    if nt == 'ndb_mgmd':
        return MgmdNode(ndict, c)
    elif nt == 'ndbd' or nt == 'ndbmtd':
        return NdbdNode(ndict, c)
    elif nt == 'mysqld':
        return MysqldNode(ndict, c)
    else:
        raise Exception('Unsupported node type: '+nt+ ' ndict='+str(ndict))
    
#def proc_node_factory(proc, c):
#    nt = proc['file']['name']
#    ndict = {'HostName':proc['file']['host'], 'NodeId': None, 'PortNumber': 0}
#    if nt == 'ndb_mgmd':
#        return MgmdNode(ndict, c)
#    elif nt == 'ndbd' or nt == 'ndbmtd':
#        return NdbdNode(ndict, c)
#    elif nt == 'mysqld':
#        return MysqldNode(ndict, c)
#    else:
#        raise Exception('Unsupported node type: '+nt+ ' ndict='+str(ndict))
    

def create_cluster_from_config_ini(path):
    cp = config_parser.parse_config_ini(path)
    cluster = Cluster([produce_ABClusterHost(h, '', '') for h in config_parser.get_option_value_set(cp, 'HostName')])
    cluster.nodes = [node_factory(ndict, cluster) for ndict in config_parser.get_node_dicts(cp, cluster.portbase+20)]
    return cluster

#def create_cluster_from_startClusterReq(req):
#    cluster = Cluster([ClusterHost(proc['file']['hostName']) for proc in req['body']['procs']])
#    cluster.nodes = [proc_node_factory(proc, cluster) for proc in req['body']['procs']]
    
class Cluster(object):
    def __init__(self, hosts, dstdir='/export/home/tmp/_ndbdev/mysql-cluster-data', basedir='/usr/local/cluster-mgt/cluster-7.1.19', portbase=41500):
        self.hosts = hosts
        self.nodes = []
        self.dstdir = dstdir
        self.basedir = basedir
        self.portbase = portbase
    
        if dstdir:
            for ch in self.hosts:
                if ch.file_exists(dstdir):
                    ch.rm_r(dstdir)
                ch.mkdir_p(dstdir)
                
            
    def deploy_config(self, src, dst):
        for node in filter(lambda n: isinstance(n, MgmdNode), self.nodes):
            node.deploy_config(src, dst)
    
    def get_deploy_config_msgs(self, localpath='example_config.ini'):
        ini = open(localpath)
        inicontent = ini.read()
        ini.close()
        return [ mock_msg('createFileReq', {'ssh': {'user': '', 'pwd':''},
                                     'file': {'hostName': mgmd.clusterhost.host, 'path': mgmd.datadir, 'name': 'config.ini' },
                                     'contentString': inicontent }) for mgmd in filter(lambda n: isinstance(n, MgmdNode), self.nodes) ]
    
    def get_start_msg(self):
        return mock_msg('startClusterReq', {'ssh': {'user': '', 'pwd':''},
                                            'procs': [ proc for cn in self.nodes for proc in cn.get_procs() ],
                                            'pgroups': [ {'plist': [mp for mn in 
                                                                    filter(lambda cn: isinstance(cn, MgmdNode), self.nodes) 
                                                                    for mp in mn.get_procs() ], 'syncPolicy': None },
                                                         {'plist': [np for nn in 
                                                                    filter(lambda cn: isinstance(cn, NdbdNode), self.nodes) 
                                                                    for np in nn.get_procs()], 'syncPolicy': {'type': 'wait', 'length': 30.0 } },
                                                          {'plist': [myp for myn in 
                                                                    filter(lambda cn: isinstance(cn, MysqldNode), self.nodes) 
                                                                    for myp in myn.get_procs()], 'syncPolicy': {'type': 'wait', 'length': 30.0 }}
                                                        ]})
                                                    
    
    def start_nodes(self, nodetype):
        map(ClusterNode.start, 
            filter(lambda n: isinstance(n, nodetype), self.nodes))
                
    def drop(self):
#        try:
        mysqlds = filter(lambda n: isinstance(n, MysqldNode), self.nodes)
        print mysqlds
        map(MysqldNode.stop, mysqlds)
#        except:
#            traceback.print_exc()
            
        for node in filter(lambda n: isinstance(n, MgmdNode) and n.is_started,
                            self.nodes):
                node.stop_cluster()
                break           
        map(lambda n: n.drop(), self.nodes)
        map(lambda h: h.drop([self.dstdir]), self.hosts)
        
    def ndb_config_xml(self, hi = 0):
        ndb_config = self.hosts[hi].auto_complete(self.basedir, ['bin'], 'ndb_config')
        return self.hosts[hi].exec_blocking([ndb_config, '--configinfo', '--xml'])


