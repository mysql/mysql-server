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

""" Tools for parsing and extracting information from cluster config.ini files. """
import StringIO
import ConfigParser

def parse_cluster_config_ini(path):
    with open(path) as ini:
        return parse_cluster_conifg_ini_(ini)

def parse_cluster_conifg_ini_(ini):
    c = []
    s = None
    for l in map(str.rstrip, ini):
        if l == '' or l.startswith('#'):
            continue
        if l.startswith('['):
            s = { 'name': l[1:-1], 'options': {} }
            c.append(s)
            continue
        (k,v) = l.split('=', 1)
        s['options'][k] = v
    return c

def write_cluster_config_ini(c):
    return '\n'.join([ '[{0}]\n{1}\n'.format(s['name'], '\n'.join(['{0}={1}'.format(k, s['options'][k]) for k in s['options'].keys()])) for s in c ])

def parse_cluster_config_ini_x(path):
    c = {}
    with open(path) as ini:
        key = None
        opts = {}
        for l in map(str.rstrip, ini):
            if l == '' or l.startswith('#'):
                continue
            if l.startswith('['):
                if key is not None:
                    c[key] = opts
                key = l
                continue
            (k,v) = l.split('=', 1)
            opts[k] = v
            if k == 'NodeId':
                key = (key, v)
    return c

# Below is deprecated
def parse_config_ini(path):
    ini = open(path)
    buf = StringIO.StringIO()
    sections = {}

    for l in map(str.rstrip, ini):
        if not 'DEFAULT' in l and len(l) > 2 and l[0] == '[' and l[-1] == ']':
            section = l[1:-1]
            n = 0
            if sections.has_key(section):
                n = sections[section] + 1
            sections[section] = n
            buf.write('[' + section + '_' + repr(n) + ']\n')
        else:
            buf.write(l+'\n')
    ini.close()
    buf.seek(0)
    
    cp = ConfigParser.ConfigParser()
    cp.optionxform = str
    cp.readfp(buf)
    buf.close()
    return cp


def get_option_value_set(cp, option):
    return set([cp.get(s, option) for s in filter(lambda s: cp.has_option(s, option), cp.sections())])

def get_node_dicts(cp, portbase):
    node_sections = filter(lambda s: cp.has_option(s, 'NodeId') and not 'API' in s, cp.sections())
    ndicts = []
    for ns in node_sections:
        t = '_'.join(ns.split('_')[:-1])
        nalist = [('_NodeType', t.lower())]
        if t == 'MYSQLD':
            nalist += [('_MysqlPort', portbase)]
            portbase += 1
        if cp.has_section(t+' DEFAULT'):
            nalist += cp.items(t+' DEFAULT')
        
        nalist += cp.items(ns)
        ndicts.append(dict(nalist))
    return ndicts

def get_actual_section(s):
    if 'DEFAULT' in s:
        return s
    return '_'.join(s.split('_')[:-1])

def get_proct1(s):
    if 'DEFAULT' in s:
        s = s.rstrip(' DEFAULT')
    else:
        s = '_'.join(s.split('_')[:-1])
    return s.lower()

def get_pid1(cp, s):
    if cp.has_option(s,'NodeId'):
        return cp.get(s, 'NodeId')
    return None

def get_ndbconnecturl(cp):
    return ','.join(["{0}:{1}".format(cp.get(s, 'HostName'), cp.get(s, 'PortNumber') ) for s in filter(lambda se: 'NDB_MGMD_' in se, cp.sections())])

def get_configvalues(cp):
    return [ {'section': get_actual_section(s), 'key': k, 'proct1': get_proct1(s), 'pid1': get_pid1(cp,s), 'proct2':None, 'pid2':None, 'val': v} for s in cp.sections() for (k,v) in cp.items(s) ]

def get_processes(cp):
    return [ {'desired_process_status': 0,
              'restartlevel': 0, 
              'xtraoptions': 0,
              'processname': get_proct1(s), 
              'internalid': get_pid1(cp,s), 
              'package': {}, 
              'hostaddress': cp.get(s, 'HostName'),
              'configfilepath':None,
              'ndbconnecturl': get_ndbconnecturl(cp)} for s in filter(lambda s: cp.has_option(s, 'NodeId') and not 'API' in s, cp.sections()) ]

    
