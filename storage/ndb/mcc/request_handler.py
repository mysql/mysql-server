# Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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

"""
The core of the configurator backend.

Implements the BaseHTTPRequestHandler that will process HTTP requests
received by the BaseHTTPServer to create the configurator backend web server.
Also contains functions and classes needed to implement the Json-based
communication protocol used to communicate with the frontend.
"""

import traceback
import SocketServer
import BaseHTTPServer
import json
import mimetypes
import sys
import types
import ssl
import copy
import socket
import os
import os.path
import logging
import optparse
import webbrowser
import tempfile
import threading
import Queue
import errno
#
import subprocess
import posixpath
import platform
#
import base64
import cryptography
from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.backends import default_backend
#
import paramiko
#
import util
from util import _parse_until_delim, parse_properties
from util import is_port_available
from util import bcolors
import config_parser
import mcc_config
#
from clusterhost import produce_ABClusterHost
from clusterhost import ExecException
from remote_clusterhost import cleanup_connections
from remote_clusterhost import list_remote_hosts
from remote_clusterhost import delete_connection
from remote_clusterhost import RemoteExecException

_logger = logging.getLogger(__name__)
CONFIGFILE = None
PASSPHRASE = None
CTRLCREC = False
PID_FILE = None
'''
List holding active MGMT connection objects to different hosts.
MGMTHostInfo is index into this list. If HOST is 12.34.56.78, it's entry in
MGMTHostInfo will be {12.34.56.78:positionInsideList (say 1)}. So to access this
host we will use MGMTConn[1].
'''
MGMT_HOST_INFO = dict()
MGMT_CONN = []
MGMT_CONN_LOCK = threading.Lock()

def worker_func_from_local(cmd, pending, done):
    """Function to run commands from local to remote hosts."""
    try:
        while True:
            # Get the next address to send command to.
            address = pending.get_nowait()
            cmd_exec = subprocess.Popen(cmd + [address], stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
            out, error = cmd_exec.communicate()
            # Output the result to the 'done' queue.
            done.put((out, error))
    except Queue.Empty:
        # No more addresses.
        pass
    finally:
        # Tell the main thread that a worker is about to terminate.
        done.put(None)

def handle_pingRemoteHostsReq(req, body):
    """
    Returns semicolon string of remote hosts and whether or not they responded to a ping, or
    'ERROR'.
    Remember that a host may not respond to a ping (ICMP) request even if the
    host name is valid.
    """
    # Option for the number of packets as a function of
    param = '-n' if platform.system().lower() == 'windows' else '-c'
    _logger.debug("RHD--> Got hostlist %s for ping.", body['data'])
    host_list = []
    if len(body['data']) < 5:
        # Invalid request. Should not happen.
        _logger.warning(bcolors.WARNING + "RHD--> handle_pingRemoteHostsReq, empty hostlist!" + \
            bcolors.ENDC)
        return make_rep(req, {'reply_type': 'ERROR'})

    host_list = body['data'].split(";") # Split data into list over ";"
    _logger.debug("RHD--> Hostlist len to PING is %s.", len(host_list))
    # Is there anything to send?
    if not host_list or host_list[0] == '':
        # Invalid request. Should not happen.
        wrn_msg = bcolors.WARNING + "RHD--> handle_pingRemoteHostsReq, no hosts in list!" + \
            bcolors.ENDC
        _logger.warning(wrn_msg)
        return make_rep(req, {'reply_type': 'ERROR'})

    # Weed out localhost(s).
    host_list[:] = [x for x in host_list if not is_host_local(x)]
    host_list = [x for x in host_list if x != '']
    wrn_msg = bcolors.WARNING + "RHD--> New hostlist to PING is %s." + bcolors.ENDC
    _logger.warning(wrn_msg, host_list)
    command = ['ping', param, '1'] # Maybe send packets twice?
    # The queue of addresses to ping.
    pending = Queue.Queue()
    # The queue of results.
    done = Queue.Queue()
    # Create all the workers.
    workers = []
    for _ in range(len(host_list)):
        workers.append(threading.Thread(target=worker_func_from_local,
                                        args=(command, pending, done)))
    # Put all the addresses into the 'pending' queue.
    for x in xrange(len(host_list)):
        pending.put(host_list[x])

    # Start all the workers.
    for w in workers:
        w.daemon = True
        w.start()

    # Collect results as they arrive.
    num_terminated = 0
    rep = ""
    # Description of result variable (res):
    # FAIL:
    # res[0]: Pinging 129.146.59.24 with 32 bytes of data:
    # Request timed out.
    # res[1]: Ping statistics for 129.146.59.24:
    #     Packets: Sent = 1, Received = 0, Lost = 1 (100% loss),
    # SUCCESS:
    # res[0]: Pinging www.google.com [172.217.20.4] with 32 bytes of data:
    # Reply from 172.217.20.4: bytes=32 time=19ms TTL=52
    # res[1]: Ping statistics for 172.217.20.4:
    #     Packets: Sent = 1, Received = 1, Lost = 0 (0% loss),
    while num_terminated < len(host_list):
        res = done.get()
        if res is None:
            # A worker is about to terminate.
            num_terminated += 1
        else:
            if res[0].find('Reply ') >= 0:
                if platform.system().lower() == 'windows':
                    rep = rep + res[0].split('Pinging ')[1].split(' with ')[0] + " is UP;"
                    _logger.debug("RHD--> %s is UP", \
                        res[0].split('Pinging ')[1].split(' with ')[0])
                else:
                    rep = rep + res[0].split('PING ')[1].split(': ')[0] + " is UP;"
                    _logger.debug("RHD--> %s is UP", \
                        res[0].split('PING ')[1].split(': ')[0])
            else:
                if platform.system().lower() == 'windows':
                    _logger.debug("RHD--> %s is not responding to ping", \
                        res[0].split('Pinging ')[1].split(' with ')[0])
                    rep = rep + res[0].split('Pinging ')[1].split(' with ')[0] + \
                        " is not responding to ping;"
                else:
                    _logger.debug("RHD--> %s is not responding to ping", \
                        res[0].split('PING ')[1].split(': ')[0])
                    rep = rep + res[0].split('PING ')[1].split(': ')[0] + \
                        " is not responding to ping;"
    # Wait for all the workers to terminate.
    for w in workers:
        w.join()
    _logger.debug("RHD--> Hosts to ping %s, done %s", len(host_list), num_terminated)
    return make_rep(req, {'reply_type': rep})

def handle_listRemoteHostsReq(req, body):
    """
    Syncing routine between FE and BE.
    FE sends list of hosts from user configuration. Match that against list of permanent connections
    to remote hosts to check if they match. When permanent connection in BE does not match any of
    the hosts in configuration, destroy it. New permanent connection(s) will be created upon
    arrival of the next command towards new host.
    req - top level message object
    body - shortcut to the body part of the message
    msg = {...
        body: {
            data: "hostname;hostname,..."
        }};
    """
    data = body['data']
    _logger.debug("RHD--> Got hostlist %s.", data)
    host_list = []
    if len(data) > 5:
        # Split data into list over ";"
        host_list = data.split(";")
        _logger.debug("RHD--> Hostlist len is %s.", len(host_list))
        if host_list:
            _logger.debug("RHD--> Weeding out localhosts.")
            # Weed out localhost(s).
            host_list[:] = [x for x in host_list if not is_host_local(x)]
            # Remove empty entries.
            host_list = [x for x in host_list if x != '']
            _logger.debug("RHD--> New hostlist is %s.", host_list)
        else:
            # Invalid request. Should not happen.
            wrn_msg = bcolors.WARNING + "RHD--> handle_listRemoteHostsReq ERROR1" + bcolors.ENDC
            _logger.warning(wrn_msg)
            return make_rep(req, {'reply_type': 'ERROR'})

        # Is there anything to send?
        if host_list:
            perm_array = list_remote_hosts()
            if perm_array:
                # If there is entry in permArray which does not correspond to anything in hostList,
                # we have discrepancy and that entry should be deleted as it is no longer part of
                # configuration.
                _logger.debug("RHD--> PermConn array len is %s.", len(perm_array))
                for x in xrange(len(perm_array)):
                    if perm_array[x] not in host_list:
                        delete_connection(perm_array[x])
                _logger.debug("RHD--> New PermConn array len is %s.", len(perm_array))
                # The other way around is not a problem as new permanent connection
                # will be created upon next request.
                return make_rep(req, {"exitstatus": 0, "err": [""], "out": host_list})
            # No permanent connections, BE will take care of this upon new requests.
            # Reply OK as there is no discrepancy.
            _logger.debug("RHD--> There was no permanent connections.")
            return make_rep(req, {"exitstatus": 0, "err": [""], "out": host_list})
        #Reply OK as all hosts were local in one way or the other.
        _logger.debug("RHD--> hostlist was all localhost.")
        return make_rep(req, {"exitstatus": 0, "err": [""], "out": "localhost"})
    else:
        #Invalid request. Should not happen.
        _logger.warning(bcolors.WARNING + "RHD--> handle_listRemoteHostsReq ERROR2" + bcolors.ENDC)
        return make_rep(req, {'reply_type': 'ERROR'})

def check_connected(sshcl):
    """Support routine checking validity of paramiko.SSHClient() connection."""
    if sshcl is not None:
        try:
            transport = sshcl.get_transport()
            if transport is not None:
                transport.send_ignore()
                return True
            return False
        except Exception: #EOFError?
            return False
    else:
        return False

def cleanup_mgmt_connections():
    """Clean up array of permanent management connections (SSH)."""
    global MGMT_CONN
    global MGMT_HOST_INFO
    global MGMT_CONN_LOCK
    with MGMT_CONN_LOCK:
        _logger.warning(bcolors.WARNING + "RHD--> Cleaning up MGMTconn. array." + bcolors.ENDC)
        if len(MGMT_CONN) >= 1:
            try:
                for x in xrange(len(MGMT_CONN)):
                    MGMT_CONN[x].close()
            except paramiko.SSHException:
                # Nothing. It is closed.
                _logger.debug("RHD--> Cleaning up MGMTconn. array, SSH exception.")
        _logger.debug("RHD--> Cleaning up MGMTconn. array index, GC.")
        del MGMT_CONN[:]
        MGMT_HOST_INFO.clear()

def setup_mgmt_tunnel(host, keybased, usernm, pwd, keyfile):
    """Function to set up MGMD connection based on index. On failure, returns False effectively
     forcing usage old HOST:PORT code."""
    global MGMT_HOST_INFO
    global MGMT_CONN
    global CTRLCREC # is web server stopping?

    if CTRLCREC:
        return False
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    result = sock.connect_ex((host, 22))
    sock.close() # we need socket we can manipulate in SSHClient object, this is just a test.
    if result > 0:
        _logger.error(bcolors.FAIL + "\nRHD--> No SSH service at %s." + bcolors.ENDC, host)
        return False

    ndx = MGMT_HOST_INFO[host]
    MGMT_CONN[ndx].load_system_host_keys()
    MGMT_CONN[ndx].set_missing_host_key_policy(paramiko.AutoAddPolicy())
    if keybased is False:
        _logger.debug('RHD--> ' + "MGMTCEnter ordinary conn.")
        try:
            MGMT_CONN[ndx].connect(hostname=host, username=usernm, password=pwd,
                                   pkey=None, look_for_keys=False, banner_timeout=4, timeout=5)
            return True
        except paramiko.SSHException:
            _logger.warning(bcolors.WARNING + "RHD--> Opening MGMT SSH conn. "
                            "to %s failed with:\n%s" + bcolors.ENDC, host, traceback.format_exc())
        except socket.error:
            _logger.error(bcolors.FAIL + "\nRCH--> Socket error." +bcolors.ENDC)
    else:
        # We have key file pointed out for us.
        privatekeyfile = keyfile or  os.path.expanduser('~/.ssh/id_rsa')
        # Is it valid file?
        if not os.path.isfile(privatekeyfile):
            _logger.error(bcolors.FAIL + "\nRHD--> %s does not exist!" + bcolors.ENDC,
                          privatekeyfile)
        try:
            mykey = paramiko.RSAKey.from_private_key_file(privatekeyfile, password=pwd)
        except paramiko.ssh_exception.SSHException as e:
            _logger.error(bcolors.FAIL + "\nRCH--> %s" + bcolors.ENDC, e.message)

        _logger.debug('RHD--> ' + "MGMTCEnter %s used.", privatekeyfile)
        try:
            MGMT_CONN[ndx].connect(hostname=host, username=usernm, pkey=mykey, look_for_keys=False,
                                   banner_timeout=4, timeout=5)
            return True
        except paramiko.SSHException:
            _logger.warning(bcolors.WARNING + "RHD--> Opening MGMT SSH conn.  to %s failed "
                            "with:\n%s" + bcolors.ENDC, host, traceback.format_exc())
        except socket.error:
            _logger.error(bcolors.FAIL + "\nRCH--> Socket error." +bcolors.ENDC)
    return False

def setup_mgmt_conn(host, keybased, user_name, pwd, keyfile, inst_dir, os_name):
    """Function to instantiate MGMD connection (or provide index of already active connection to
     requested host). Returns MGMD connection ID. Return value of -1 indicates LOCALHOST or, in case
     of ERROR, it will use old code to try to establish socket connection via HOST:PORT pair.
     Uses setup_mgmt_tunnel to create new connection."""
    global MGMT_CONN
    global MGMT_HOST_INFO
    global MGMT_CONN_LOCK
    global CTRLCREC
    if CTRLCREC:
        return -2

    with MGMT_CONN_LOCK:
        # If the host is local, skip and use old code.
        if not is_host_local(host):
            if host not in MGMT_HOST_INFO: #We need MGM connection for this host.
                idx = len(MGMT_HOST_INFO)
                # Fill in MGMTHostInfo dict and initiate connection.
                MGMT_HOST_INFO[host] = idx
                _logger.debug('RHD--> ' + "MGMTChost="+host)
                if user_name is not None:
                    _logger.debug('RHD--> ' + "MGMTCusername="+user_name)
                if pwd is not None:
                    if pwd.startswith('**'):
                        pwd = None
                    else:
                        _logger.debug('RHD--> ' + "MGMTCpwd=**")
                MGMT_CONN.append(paramiko.SSHClient())

                if keybased is not None:
                    _logger.debug('RHD--> ' + "MGMTCkeybased="+str(keybased))
                if keyfile is not None:
                    _logger.debug('RHD--> ' + "MGMTCkeyfile="+keyfile)
                if not setup_mgmt_tunnel(host, keybased, user_name, pwd, keyfile):
                    # Clean up array entry.
                    MGMT_HOST_INFO.pop(host, None) # DICT_RM Key=Host, Default=None
                    MGMT_CONN.pop() # Remove last added to array/list.
                    # Fall back to host:port code.
                    _logger.warning(bcolors.WARNING +
                                    "RHD--> Falling back to HOST:PORT conn!" + bcolors.ENDC)
                    idx = -1
                else:
                    # Host was in list. Add full path to command to execute.
                    stat_res = ""
                    binary_name = "ndb_mgm"
                    if os_name == "Windows":
                        binary_name = "ndb_mgm.exe"
                    locations = ['bin', 'sbin', 'scripts', '']
                    # Add full path to ndb_mgm for remote host.
                    # There might be a problem here. Use TRY.
                    _sftpc = MGMT_CONN[idx].open_sftp()
                    for l in locations:
                        choice = posixpath.join(inst_dir, l, binary_name)
                        _logger.debug("RHD--> choice is %s", choice)
                        try:
                            stat_res = _sftpc.stat(choice)
                            stat_res = choice
                            break
                        except IOError as ioerr:
                            if ioerr.errno == errno.ENOENT:
                                stat_res = ""
                    if stat_res == "":
                        stat_res = binary_name
                    _logger.debug("RHD--> fullCmd is %s for ConnIdx %s", stat_res, idx)
                    MGMT_CONN[idx].fullCmd = stat_res
                    _sftpc.close()
            else:
                # There has to be check for changed credentials here to make it more robust. Right
                # now, we invalidate conn.array by calling deploymenttree::stopStatusPoll(reason)
                # with STOP from wizard::~LN800. It is OK for few hosts but for like 10 it just
                # might become too slow to reconnect all.
                _logger.debug("RHD--> host %s is already in at [%s]", host, MGMT_HOST_INFO[host])
                # this call might take a while to complete
                if check_connected(MGMT_CONN[MGMT_HOST_INFO[host]]):
                    idx = MGMT_HOST_INFO[host]
                else:
                    if not setup_mgmt_tunnel(host, keybased, user_name, pwd, keyfile):
                        # there can be at most 2 of these connections so no big deal
                        _logger.warning(bcolors.WARNING + "RHD--> host %s is already in  at [%s] " +
                                        "but not alive. Cleaning up." + bcolors.ENDC, host,
                                        MGMT_HOST_INFO[host])
                        # Clean up array entry.
                        cleanup_mgmt_connections()
                        # Fall back to host:port code.
                        _logger.warning(bcolors.WARNING +
                                        "RHD--> Falling back to HOST:PORT conn!" + bcolors.ENDC)
                        idx = -1
        else:
            idx = -1
        return idx # Return connection index.

class ShutdownException(Exception):
    """Exception thrown when shutdown command arrives"""
    pass

class ReplyJsonEncoder(json.JSONEncoder):
    """Specialized encoder for which will serialize the following types, in addition to those
     handled by JSONEncoder:
    TypeTypes - as an html-friendly version their str() output
    TracebackType - as a list of elements (returned by traceback.extrace_tb
    Any class with the __dict__ attribute is serialized as a dict to a json object."""

    def default(self, obj):
        """Overrides the default function in JSONEncoder. Specialized for TypeTypes,
        TracebackTypes and dict-like types. All other types are passed to JSONEncoder.default()."""

        if isinstance(obj, types.TypeType):
            return str(obj).replace('<', '&lt;').replace('>', '&gt;')
        if isinstance(obj, types.TracebackType):
            return traceback.extract_tb(obj)
        if hasattr(obj, '__dict__'):
            assert isinstance(vars(obj), dict), str(type(obj)) + ' dict attr has type ' + \
                str(type(vars(obj)))
            return vars(obj)
        # Ordinary json serialization
        return json.JSONEncoder.default(self, obj)

def handle_req(req):
    """Primary dispatcher function for messages from the client-frontend. Uses introspection to
     look for a function named handle_<command name> and invokes that to process the message.
    req - incoming message (web server request) from the frontend"""
    h = globals()['handle_'+req['head']['cmd']]
    return h(req, req['body'])

def make_rep(req, body=None):
    """Utility which creates a reply object based on the headers in the request
    object."""
    _logger.debug('RHD--> make_rep')
    rep = {'head': {'seq': req['head']['seq'] +1,
                    'cmd': req['head']['cmd'],
                    'rSeq': req['head']['seq']}}
    if body:
        rep['body'] = body
    return rep

def is_host_local(host_designation):
    """
    Function which detects if input represents localhost.

    Arguments:
        host_designation {[string]} -- [IPaddress or FQDN or special]

    Returns:
        [boolean] -- [is localhost or not]
    """
    shn = socket.gethostbyname_ex(socket.gethostname())
    #('hostxx.domain.company.com', [], ['10.172.162.24', 'next address', ...])
    # _logger.debug('RHD--> Produce ABC socket host name is %s.', shn)
    if host_designation == 'localhost' or host_designation == '127.0.0.1' or \
        host_designation == shn[0]:
        _logger.debug('RHD--> Host is local (1-1).')
        return True
    for x in range(len(shn[2])):
        if host_designation == shn[2][x]:
            return True
    # If all of the above fails, we can safely assume host is not local. No need for further check.
    return False

def get_cred(host_name, body):
    """Get the credentials from the message in the form of a (user, pwd) tuple.
    If there is no ssh object present, or keyBased is present and True, return
    (User, passphrase, key_file) block."""
    try:
        if (not body.has_key('ssh') or util.get_val(body['ssh'], 'keyBased', False)) and \
                (not is_host_local(host_name)):
            # It's key-based, implement new logic.
            _logger.debug('RHD--> get_cred, new logic 1.')
            return (True, body['ssh']['key_user'], body['ssh']['key_passp'],
                    body['ssh']['key_file'])
    except KeyError:
        if (not body.has_key('ssh') or util.get_val(body['ssh'], 'keyBased', False)) and \
            (not is_host_local(host_name)):
            # It's key-based, implement new logic.
            _logger.debug('RHD--> get_cred, new logic 2.')
            return (True, body['ssh']['key_user'], body['ssh']['key_passp'],
                    body['ssh']['key_file'])
    _logger.debug('RHD--> get_cred, old(ish) code.') #No creds at all.
    if is_host_local(host_name):
        return (False, "", "", None)
    _logger.debug('RHD--> get_cred, old(ish) code with PWD.')
    return (False, body['ssh']['user'], body['ssh']['pwd'], None)

def get_key(password):
    """Get passphrase to decrypt old configuration files."""
    _logger.debug('RHD--> Getting key from passphrase.')
    digest = hashes.Hash(hashes.SHA256(), backend=default_backend())
    digest.update(bytes(password))
    return base64.urlsafe_b64encode(digest.finalize())

#Leave because of old, encrypted configurations.
def decrypt(key, token):
    """Decrypt old configuration files."""
    f = Fernet(get_key(key))
    _logger.debug('RHD--> Decrypting.')
    return f.decrypt(bytes(token))

def handle_hostInfoReq(req, body):
    """Handler function for hostInfoReq commands. Will connect to the specified
    host through a remote.ClusterHost object to retrieve information.
    req - top level message object
    body - shortcut to the body part of the message
    """
    (key_based, user, pwd, key_file) = get_cred(body['hostName'], body)
    _logger.debug('RHD--> Handling HostInfoRequest.')
    with produce_ABClusterHost(body['hostName'], key_based, user, pwd, key_file) as ch:
        return make_rep(req, {
            'host': {
                'name': ch.host
            },
            'hostRes': {
                'ram': ch.ram,
                'cores': ch.cores,
                'uname': ch.hostInfo.uname,
                'installdir': ch.installdir,
                'datadir': ch.hostInfo.pm.join(ch.homedir, 'MySQL_Cluster'),
                'diskfree': ch.hostInfo.disk_free,
                'fqdn': socket.getfqdn(ch.host),
                'osver': ch.hostInfo.osver,
                'osflavor': ch.hostInfo.osflavor,
                'docker_info': ch.hostInfo.docker_info,
                'intip': ch.hostInfo.intip,
                'home': ch.homedir
            }
        })

def handle_hostDockerReq(req, body):
    """Handler function for hostDockerReq command. Will connect to the specified
    host through a remote.ClusterHost object to retrieve information.
    req - top level message object
    body - shortcut to the body part of the message
    """
    (key_based, user, pwd, key_file) = get_cred(body['hostName'], body)
    with produce_ABClusterHost(body['hostName'], key_based, user, pwd, key_file) as ch:
        return make_rep(req, {
            'host': {'name': ch.host},
            'hostRes': {'DockerInfo':ch.hostInfo.docker_info}
        })

def start_proc(proc, body):
    """Start individual process as specified in startClusterReq command.
    proc - the process object in the message
    body - the whole message
    """
    f = proc['file']
    (key_based, user, pwd, key_file) = get_cred(f['hostName'], body)
    with produce_ABClusterHost(f['hostName'], key_based, user, pwd, key_file) as ch:
        pc = proc['procCtrl']
        params = proc['params']
        if f.has_key('autoComplete'):
            _logger.debug('RHD--> Autocomplete key is there')
            if isinstance(f['autoComplete'], list):
                _logger.debug('RHD--> Autocomplete key is a list.')
                executable = ch.auto_complete(f['path'], f['autoComplete'], f['name'])
            else:
                _logger.debug('RHD--> Autocomplete key is not a list.')
                executable = ch.auto_complete(
                    f['path'], ['', 'bin', 'sbin', 'scripts',
                                ch.path_module.join('..', 'scripts')], f['name'])
        else:
            _logger.debug('RHD--> No autocomplete key')
            executable = ch.path_module.join(f['path'], f['name'])

        stdin_file = None
        if f.has_key('stdinFile'):
            assert (ch.file_exists(f['stdinFile'])), 'File ' + f['stdinFile'] + \
                " does not exist on host " + ch.host
            stdin_file = f['stdinFile']

        cmdv = util.params_to_cmdv(executable, params)
        if proc.has_key('isCommand'):
            _logger.debug('RHD--> Attempting to launch ' + executable +
                          ', (cmd)CMDV is '+str(cmdv))
            return ch.execute_command(cmdv, stdin_file)

        _logger.debug('RHD--> Attempting to launch %s, (!cmd)CMDV is %s', executable, cmdv)
        # Check if it's NET STOP!
        # msg.procCtrl.kill = true; If there.
        # msg.procCtrl.daemonWait = 10; If there.
        # msg.procCtrl.nodeid = nodeid;
        # msg.procCtrl.getPID = command to get PID of process to kill.
        # For WINDOWS:
        # msg.procCtrl.servicename = Name of the service to query for PID.
        # For UNIX DNODE process, command will be (f['name']) "DNODE". It does
        # not have proper command since DNODE process is not a service.
        # For Windows (services), command will be (f['name']) "NET" or "SC".

        # Determine if command has KILL member (pc.has_key('kill'))
        # and if it's Windows (pc.has_key('servicename')).

        # If there is no KILL member, execute normally (last else), propagate
        # eventual failure to front end.
        # If there is KILL member, execute normal command within TRY...CATCH
        # and if it fails, execute procCtrl.getPID and pass it to KILL command
        # which we hardcode here:
        # o  UNIX     kill -9 PID
        # o  WINDOWS  taskkill /F /PID PID
        # Issue KILL as normal command so that eventual error gets propagated
        # back to front end.

        # Special case is when pc["NodeId"] == 49 (1st management node) and we
        # need to kill the process (i.e. normal shutdown which takes down all
        # Cluster processes failed). This should signal we need to kill
        # remaining MGMT node (if any) and all the data node processes too
        # to take Cluster down.
        # This happens in 2 ways, depending on do we have 2nd MGMT process or not:
        # 1) If we have 2nd MGMT process, we will issue ndb_mgm -e'SHUTDOWN' there
        # and wait to see if it succeeded. If not we will kill this process too.
        # 2) If we do not have 2nd MGMT process or it failed to stop Cluster we
        # have stray data node processes to kill. These processes do not have
        # "normal" stop command on Linux (on Windows we have NET STOP Nx) but
        # just kill. For Windows we will do NET STOP + taskkill /PID. Front end
        # should make sure, by examining the output of ndb_mgm, to send DNODE
        # kill commands.

        # Since ndbmtd is started as daemon, we have two to kill:
        # cat ndb_1_out.log | grep "Angel pid:"cat ndb_1_out.log | grep "Angel pid:" | tail -n1
        # 2019-02-14 11:37:11 [ndbd] INFO     -- Angel pid: 16104 started child: 16105
        # [opc@instance-1100 ~]$ cat MySQL_Cluster/1/ndb_1_out.log | grep "Angel pid:" |
        # tail -n1 | cut -d ':' -f5 16105

        if not pc.has_key('kill'):
            _logger.debug('RHD--> >>> START_PROC.')
            return ch.exec_cmdv(cmdv, pc, stdin_file)
        rd = None
        # RemoteClusterHost / LocalClusterHost
        htype = str(ch.__class__.__name__)
        if pc.has_key('servicename'):
            # WINDOWS NET/SC STOP. This should wait 2 seconds before trying to return.
            if pc.has_key('getPIDchild'):
                # This is DATANODE process.
                try: # regular
                    _logger.debug('RHD--> >>> Sending NET STOP with kill member.')
                    rd = ch.exec_cmdv(cmdv, pc, stdin_file)
                    # was there an error?
                    if rd.find("errcode:") == -1:
                        return rd
                    _logger.warning(bcolors.WARNING +
                                    "RHD--> >>> Command failed with %s" + bcolors.ENDC, rd)
                except (ExecException, RemoteExecException) as e:
                    # ExecException for localHost, RemoteExecException for remoteHost
                    _logger.warning(bcolors.WARNING + "RHD--> >>> NET STOP failed with "
                                    "Exception %s " + bcolors.ENDC, str(e))

                # If we made it to here, there was a failure of some kind.
                pc['daemonWait'] = 1
                pid_get = util.get_val(pc, 'getPID')
                _logger.debug('RHD--> >>> PIDget command is %s', pid_get)
                try:
                    rd = ch.exec_cmdv(pid_get, pc, stdin_file)
                    if rd.find("errcode:") != -1:
                        _logger.warning(bcolors.WARNING + "RHD--> >>> Command failed with %s" +
                                        bcolors.ENDC, rd)
                        return rd
                except (ExecException, RemoteExecException) as e:
                    # There is no recovery from not knowing PID...
                    _logger.error(bcolors.FAIL + "RHD--> >>> Getting PID failed (process still "
                                  "running) with ex %s" + bcolors.ENDC, str(e))
                    return e

                if len(rd) > 10:
                    get_it = rd.split("PID                : ")[1].split("FLAGS")[0].strip()
                    _logger.debug("RHD--> PID is %s", get_it)
                    get_it = int(get_it)
                    # There is a PID, so NET STOP failed. Fish it out.
                    if get_it > 0:
                        # We have Angel process PID. Try to get child PID, ignore if fails.
                        # This can take longer on stuffed Windows box:
                        pc['daemonWait'] = 2
                        pid_get_child = 'wmic process where (ParentProcessId=' + \
                            str(get_it) +') get ProcessId /value'
                        child_pid = 0
                        _logger.debug('RHD--> >>> pidGetChild is %s', pid_get_child)
                        try:
                            rd = ch.exec_cmdv(pid_get_child, pc, stdin_file)
                            if rd.find("errcode:") != -1:
                                _logger.warning(bcolors.WARNING + "RHD--> >>> pidGetChild failed "
                                                "with %s" + bcolors.ENDC, rd)
                                child_pid = -1
                            else:
                                # WMIC does not fail but rather sends "No Instance(s) Available."
                                # When malformed command line, it will send negative result,
                                # text ERROR: on line 2 and text Description=... on line 3
                                # (index 2, of course).
                                if rd:
                                    rm = rd.split('\r')
                                    for l in rm:
                                        if l.strip() != "":
                                            if l.find("ProcessId") != -1:
                                                child_pid = int(l.split("ProcessId=")[1])
                                                break

                                else:
                                    # Very peculiar failure... Should not happen.
                                    child_pid = -1

                                if child_pid <= 0:
                                    _logger.warning(bcolors.WARNING + "RHD--> >>> Sending " + \
                                        "command (has kill), get child pid failed" + bcolors.ENDC)
                                    # We don't have both PID's, just kill Angel then. Although
                                    # DNODE processes may die on their own if no viable Cluster
                                    # can be formed.
                                    pc['daemonWait'] = 1
                                    comm = 'taskkill /F /PID ' + str(get_it)
                                    _logger.debug('RHD--> >>> Issuing %s', comm)
                                    return ch.exec_cmdv(comm, pc, stdin_file)
                                else:
                                    # Both pids there, we are on.
                                    _logger.warning(bcolors.WARNING + "RHD--> >>> " + "Sending " + \
                                        "command (has kill), getting child pid success, "+ \
                                            "pid is: %s" + bcolors.ENDC, child_pid)
                                    # IF we made it to here, we have 2 valid PID's, angel & Dnode.
                                    # We need to kill both for DNode process to go down. First,
                                    # we kill Angel process otherwise it will just restart DNode.
                                    pc['daemonWait'] = 1
                                    comm = 'taskkill /F /PID ' + str(get_it)
                                    _logger.debug('RHD--> >>> Issuing %s', comm)
                                    try:
                                        ch.exec_cmdv(comm, pc, stdin_file)
                                        if rd.find("errcode:") != -1:
                                            _logger.warning(bcolors.WARNING + "RHD--> >>> Command "
                                                            "failed with %s" + bcolors.ENDC, rd)
                                            return rd
                                        comm = 'taskkill /F /PID ' + str(child_pid)
                                        _logger.warning(bcolors.WARNING + 'RHD--> >>> ' + \
                                            'Issuing kill child process %s'+ bcolors.ENDC, comm)
                                        return ch.exec_cmdv(comm, pc, stdin_file)
                                    except (ExecException, RemoteExecException) as e:
                                        log_msg = "RHD--> >>> Sending command (has kill) kill" + \
                                        " angel/child failed with exception %s"
                                        _logger.warning(bcolors.WARNING + log_msg + bcolors.ENDC,
                                                        str(e))
                                        return e
                        except (ExecException, RemoteExecException) as e:
                            # There is no recovery from not knowing PID...
                            log_msg = bcolors.WARNING + "RHD--> >>> Sending command (has kill)," + \
                                "getting child PID failed with exception %s" + bcolors.ENDC
                            _logger.warning(log_msg, str(e))
                            return e
                    else:
                        # PID is 0 so shutdown almost complete.
                        _logger.debug('RHD--> >>> NET STOP success, though bit late.')
                        return 'OK'
                else:
                    # GetPID failed to provide good result.
                    err_msg = 'RHD--> >>> GetPID failed, process still running!'
                    _logger.error(bcolors.FAIL + err_msg + bcolors.ENDC)
                    return 'ERROR, getting PID failed with ' + str(rd)
                # Has getPIDChild end.
            else:
                # This is MANAGEMENT or SQL process.
                try: # regular
                    _logger.debug('RHD--> >>> Sending NET STOP with kill member.')
                    rd = ch.exec_cmdv(cmdv, pc, stdin_file)
                    # was there an error?
                    if rd.find("errcode:") == -1:
                        return rd
                    _logger.warning(bcolors.WARNING + "RHD--> >>> Command failed with %s" +
                                    bcolors.ENDC, rd)
                except (ExecException, RemoteExecException) as e:
                    # It's already dumped in ClusterHost:_exec_cmdv
                    wr_msg = "RHD--> >>> NET STOP failed to execute with %s"
                    _logger.warning(bcolors.WARNING + wr_msg + bcolors.ENDC, str(e))

                # If we made it to here, there was a failure of some kind.
                # Lower the daemonWait time here as kill works fast.
                pc['daemonWait'] = 1
                pid_get = util.get_val(pc, 'getPID')
                _logger.debug('RHD--> >>> PIDget command is %s', pid_get)
                try: # regular
                    rd = ch.exec_cmdv(pid_get, pc, stdin_file)
                    if rd.find("errcode:") != -1:
                        _logger.warning(bcolors.WARNING + "RHD--> >>> Command failed with %s" +
                                        bcolors.ENDC, rd)
                        return rd
                except (ExecException, RemoteExecException) as e:
                    # There is no recovery from not knowing PID...
                    log_msg = bcolors.FAIL + "RHD--> >>> Getting PID failed, process still " + \
                        "running, with exception %s" + bcolors.ENDC
                    _logger.error(log_msg, str(e))
                    return e

                if len(rd) > 10:
                    # There is a PID, so NET STOP failed. Fish it out.
                    get_it = rd.split("PID                : ")[1].split("FLAGS")[0].strip()
                    _logger.debug("RHD--> PID is %s", get_it)
                    get_it = int(get_it)
                    if get_it > 0:
                        comm = 'taskkill /F /PID ' + str(get_it)
                        _logger.debug('RHD--> >>> Issuing %s', comm)
                        pc['daemonWait'] = 1
                        return ch.exec_cmdv(comm, pc, stdin_file)
                    # PID is 0 so shutdown almost complete.
                    _logger.debug('RHD--> >>> NET STOP success, though bit late.')
                    return 'OK'
                # GetPID failed to provide good result.
                err_msg = 'RHD--> >>> GetPID failed, process still running!'
                _logger.error(bcolors.FAIL + err_msg + bcolors.ENDC)
                return 'ERROR, getting PID failed with ' + str(rd)
        else:
            # Do same for UNIX.
            _logger.debug('RHD--> >>> UNIX Start/stop, proc has kill.')
            # Try regular if not DNODE!
            if f['name'] != "DNODE":
                _logger.debug('RHD--> >>> UNIX stop, proc has kill, not DNode.')
                try: # regular
                    log_msg = 'RHD--> >>> UNIX stop, proc has kill, not DNode ' + \
                        'trying regular.'
                    _logger.warning(bcolors.WARNING + log_msg + bcolors.ENDC)
                    rd = ch.exec_cmdv(cmdv, pc, stdin_file)
                    if rd.find("errcode:") == -1:
                        return rd
                    _logger.warning(bcolors.WARNING + "RHD--> >>> Command failed with %s" +
                                    bcolors.ENDC, rd)
                except (ExecException, RemoteExecException) as e:
                    log_msg = bcolors.WARNING + "RHD--> >>> Sending command with kill member, " + \
                        "1st request failed with exception %s" + bcolors.ENDC
                    _logger.warning(log_msg, str(e))

                pid_get = util.get_val(pc, 'getPID')
                pc['daemonWait'] = 1
                try: # regular
                    _logger.debug('RHD--> >>> UNIX STOP, proc has kill, not DNode, '
                                  'regular %s failed, getting PID %s.', cmdv, pid_get)
                    if htype == 'LocalClusterHost':
                        # to expand shell because of *.pid
                        rd = ch.execfast(pid_get)
                    else:
                        rd = ch.exec_cmdv(pid_get, pc, stdin_file)
                    if rd.find("errcode:") != -1:
                        _logger.warning(bcolors.WARNING + "RHD--> >>> Command failed with %s" +
                                        bcolors.ENDC, rd)
                        return rd
                except (ExecException, RemoteExecException) as e:
                    # There is no recovery from not knowing PID...
                    wrn_msg = "RHD--> >>> Getting PID failed, process still running,"
                    _logger.warning(bcolors.WARNING + wrn_msg + " with exception %s" +
                                    bcolors.ENDC, str(e))
                    return e

                _logger.debug('RHD--> >>> UNIX stop, proc has kill, not DNode, regular %s'
                              ' failed, PID %s,  rd is %s.', cmdv, pid_get, rd)
                # on my Mac box, .bashrc has error which is returned first...
                if rd:
                    hlp_str = str(rd)
                else:
                    hlp_str = ''

                if hlp_str and int(hlp_str.strip()) > 0:
                    comm = 'kill -9 ' + hlp_str.strip()
                    _logger.debug('RHD--> >>> Issuing %s', comm)
                    return ch.exec_cmdv(comm, pc, stdin_file)
                # PID is 0 so shutdown almost complete.
                _logger.debug('RHD--> >>> ndb_mgm stop success, bit late though.')
                return 'OK'
            else:
                # DNODE process. ndb_mgm failed and we have to stop these by hand.
                # Since we run in daemon mode on *nix, we have ANGEL and child processes,
                # so more logic is needed.
                # cat MySQL_Cluster/1/ndb_1_out.log | grep "Angel pid:" | tail -n1 | cut -d ':' -f5
                pid_get = util.get_val(pc, 'getPID')
                pid_get_child = util.get_val(pc, 'getPIDchild')
                # we are missing rest of command line. was failing without appending path to
                # commands on some Linux boxes :-/
                pid_get_child += \
                    " | /usr/bin/grep 'Angel pid:' | /usr/bin/tail -n1 | /usr/bin/cut -d ':' -f5"
                _logger.debug("RHD--> >>>PidGetChild command\n%s", pid_get_child)
                child_pid = ""
                pc['daemonWait'] = 1
                # Try to get child PID, ignore if fails.
                try:
                    if htype == 'LocalClusterHost':
                        # to expand shell because of *.pid
                        rd = ch.execfast(pid_get_child)
                    else:
                        rd = ch.exec_cmdv(pid_get_child, pc, stdin_file)
                    if rd.find("errcode:") != -1:
                        wrn_msg = "RHD--> >>> pidGetChild failed with %s"
                        _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, rd)
                    else:
                        if rd:
                            hlp_str = str(rd)
                        else:
                            hlp_str = ''
                        if hlp_str and int(hlp_str.strip()) > 0:
                            comm = 'kill -9 ' + hlp_str.strip()
                            child_pid = hlp_str.strip()
                            wrn_msg = bcolors.WARNING + "RHD--> >>> Sending command (has kill)," + \
                                " getting child pid succ, pid is: %s" + bcolors.ENDC
                            _logger.warning(wrn_msg, child_pid)
                except (ExecException, RemoteExecException) as e:
                    wrn_msg = bcolors.WARNING + "RHD--> >>> Sending command (has kill), getting" + \
                       " child pid failed with exception %s" + bcolors.ENDC
                    _logger.warning(wrn_msg, str(e))

                try: # regular
                    if htype == 'LocalClusterHost':
                        # to expand shell because of *.pid
                        rd = ch.execfast(pid_get)
                    else:
                        rd = ch.exec_cmdv(pid_get, pc, stdin_file)
                    if rd.find("errcode:") != -1:
                        _logger.warning(bcolors.WARNING + "RHD--> >>> Command failed with %s" +
                                        bcolors.ENDC, rd)
                        return rd
                except (ExecException, RemoteExecException) as e:
                    # There is no recovery from not knowing PID...
                    wrn_msg = bcolors.WARNING + "RHD--> >>> Sending command (has kill), getting" + \
                       " pid failed with exception %s" + bcolors.ENDC
                    _logger.warning(wrn_msg, str(e))
                    return e

                if rd:
                    hlp_str = str(rd)
                else:
                    hlp_str = ''
                if hlp_str and int(hlp_str.strip()) > 0:
                    # There is a PID, so ndb_mgm failed. Fish it out.
                    comm = 'kill -9 ' + hlp_str.strip()
                    _logger.warning(bcolors.WARNING + 'RHD--> >>> Issuing kill Angel process %s' +
                                    bcolors.ENDC, comm)
                    if len(child_pid) <= 0:
                        return ch.exec_cmdv(comm, pc, stdin_file)
                    try:
                        rd = ch.exec_cmdv(comm, pc, stdin_file)
                        if rd.find("errcode:") != -1:
                            _logger.warning(bcolors.WARNING + "RHD--> >>> Command failed "
                                            "with %s" + bcolors.ENDC, rd)
                            return rd
                        comm = 'kill -9 ' + child_pid
                        wrn_msg = 'RHD--> >>> Issuing kill child process %s'
                        _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, comm)
                        return ch.exec_cmdv(comm, pc, stdin_file)
                    except (ExecException, RemoteExecException) as e:
                        wrn_msg = bcolors.WARNING + "RHD--> >>> Sending command (has kill), " + \
                        "angel/child  child failed with exception %s" + bcolors.ENDC
                        _logger.warning(wrn_msg, str(e))
                        return e
                else:
                    # PID is 0 so shutdown almost complete.
                    _logger.debug('RHD--> >>> Kill DNODE success, though bit late.')
                    return rd

def handle_executeCommandReq(req, body):
    """Handler function for execCommandReq messages. Runs the process specified in by the command
     property."""
    dbg_msg = copy.deepcopy(body)
    if dbg_msg['ssh'].has_key('pwd'):
        dbg_msg['ssh']['pwd'] = '*' * len(dbg_msg['ssh']['pwd'])
    if dbg_msg['ssh'].has_key('key_passp'):
        dbg_msg['ssh']['key_passp'] = '*' * len(dbg_msg['ssh']['key_passp'])
    _logger.debug('RHD--> executeCommandReq %s', dbg_msg)
    if body['command'].has_key('isCommand'):
        _logger.debug('RHD--> EXEC_CMD isCommand')
        return make_rep(req, start_proc(body['command'], body))
    return make_rep(req, {'out': start_proc(body['command'], body)})

def handle_createFileReq(req, body):
    """Handler function for createFileReq commands. Creates a file on the remote
    host with content from the message.
    req - top level message object
    body - shortcut to the body part of the message
    There is a problem with this function on Windows:
    Since pathname = ch.path_module.join(pathname, f['name'])
    DIRECTORY pathname instead of file PATH/NAME is created leading to Access violation :-/
    Alas, os.path.normpath refers to localhost :-/
    """
    _logger.debug('RHD--> handle_createFileReq')
    f = body['file']
    (key_based, user, pwd, key_file) = get_cred(f['hostName'], body)
    with produce_ABClusterHost(f['hostName'], key_based, user, pwd, key_file) as ch:
        _logger.debug('RHD--> handle_createFileReq, produced chost')
        pathname = f['path']
        if pathname.find("~") == 0:
            _logger.debug('RHD--> handle_createFileReq, expanding ~')
            pathname = os.path.expanduser(pathname)
            _logger.debug('RHD--> handle_createFileReq, new pathname is %s', pathname)

        ch.mkdir_p(pathname)
        if f.has_key('name'):
            pathname = ch.path_module.join(pathname, f['name'])
            _logger.debug('RHD--> handle_createFileReq, full pathname is %s', pathname)
            assert not (f.has_key('autoComplete') and f['autoComplete'])
            assert not (not (f.has_key('overwrite') and f['overwrite']) \
                and ch.file_exists(pathname)), 'File ' + pathname + \
                    ' already exists on host '+ch.host
            with ch.open(pathname, 'w+') as rf:
                rf.write(body['contentString'])
            with ch.open(pathname) as rf:
                assert rf.read() == body['contentString']
    _logger.debug('RHD--> pathname %s created.', pathname)
    return  make_rep(req)

def handle_appendFileReq(req, body):
    """Handler function for appendFileReq commands. Opens two files on the
    remote host, copies from source and appends to destination.
    req - top level message object
    body - shortcut to the body part of the message
    """

    assert (body.has_key('sourceFile') and body.has_key('destinationFile'))
    sf = body['sourceFile']
    df = body['destinationFile']
    assert (sf.has_key('path') and sf.has_key('name') and sf.has_key('hostName'))
    assert (df.has_key('path') and df.has_key('name') and df.has_key('hostName'))

    (key_based, user, pwd, key_file) = get_cred(sf['hostName'], body)

    with produce_ABClusterHost(sf['hostName'], key_based, user, pwd, key_file) as ch:
        sp = ch.path_module.join(sf['path'], sf['name'])
        dp = ch.path_module.join(df['path'], df['name'])

        assert (ch.file_exists(dp)), 'File ' + dp + ' does not exist on host ' + ch.host
        content = None
        with ch.open(sp) as src_file:
            content = src_file.read()

        assert (ch.file_exists(sp)), 'File ' + sp + ' does not exist on host ' + ch.host
        with ch.open(dp, 'a+') as dest_file:
            dest_file.write(content)

    return make_rep(req)

def handle_checkFileReq(req, body):
    """Handler function for checkFileReq commands. Check if a file exists on a remote host.
    req - top level message object
    body - shortcut to the body part of the message
    """
    f = body['file']
    _logger.debug('RHD--> path %s in checking', f)
    (key_based, user, pwd, key_file) = get_cred(f['hostName'], body)

    with produce_ABClusterHost(f['hostName'], key_based, user, pwd, key_file) as ch:
        sp = ch.path_module.join(f['path'], f['name'])
        assert (ch.file_exists(sp)), 'File ' + sp + ' does not exist on host ' + ch.host
    _logger.debug('RHD--> pathname %s checked', sp)
    return  make_rep(req)

def handle_dropStuffReq(req, body):
    """Handler function for RM -R, both files and directories.
    req - top level message object
    body - shortcut to the body part of the message
    body['command']['file']...
    """
    f = body['file']
    path = f['path']

    (key_based, user, pwd, key_file) = get_cred(f['host'], body)
    _logger.debug('RHD--> Handling DropStuffRequest.')
    with produce_ABClusterHost(f['host'], key_based, user, pwd, key_file) as ch:
        res = ch.rm_r(path)
        if res is None:
            _logger.debug('RHD--> DropStuffRequest, res is None')
            res = "OK."
        else:
            _logger.debug('RHD--> DropStuffRequest, res is %s', res)

        return make_rep(req, {'out': res})

def handle_listDirectoryReq(req, body):
    """Handler function for listDirectoryReq command.
    req - top level message object
    body - shortcut to the body part of the message
    """
    # This is what new session starts with so reset globals from previous run.
    global PASSPHRASE
    global CONFIGFILE
    PASSPHRASE = None
    CONFIGFILE = None
    _logger.warning(bcolors.WARNING + 'RHD--> Passphrase & ConfigFileName reset.' + bcolors.ENDC)
    f = body['file']
    results = []
    path = f['path']
    ext = f['ext']
    (key_based, user, pwd, key_file) = get_cred(f['hostName'], body)
    with produce_ABClusterHost(f['hostName'], key_based, user, pwd, key_file) as ch:
        if path == "~":
            path = ch.homedir
        path = os.path.join(path, '') # Add last /... Well, IF MCC is not running on localhost,
        #this will fail. Will fix later if needs be.
        results += [each for each in os.listdir(path) \
            if (each.endswith(ext) and os.path.getsize(path+each) > 10)]
        return make_rep(req, {
            'host': {'name': ch.host},
            'hostRes': {
                'listDirectory': results,
                'realpath': path.replace("\\", "/")
            }
        })
        # Will return / instead of \ on Windows. repr(path) would return \\

def handle_fileExistsReq(req, body):
    """Handler function for fileExistsReq command.
    req - top level message object
    body - shortcut to the body part of the message
    Plain file exists on host or not (no assert).
    """
    path = body['path']
    filename = body['fname']
    res_str = ""
    (key_based, user, pwd, key_file) = get_cred(body['hostName'], body)
    with produce_ABClusterHost(body['hostName'], key_based, user, pwd, key_file) as ch:
        _logger.debug('RHD--> Produced ABC (FER).')
        if path == "~":
            path = ch.homedir
        sp = ch.path_module.join(path, filename)
        _logger.debug('RHD--> pathname %s in checking.', sp)
        res_str = ch.file_exists(sp)
        if res_str is not None:
            result = 1
        else:
            result = 0
        _logger.debug('RHD--> FER result is %s.', result)
        return  make_rep(req, {
            'host': {'name': ch.host},
            'hostRes': {'fname': filename, 'exists': result}})

def handle_cffileExistsReq(req, body):
    """Handler function for configfileExistsReq command.
    req - top level message object
    body - shortcut to the body part of the message
    Plain file exists on host or not.
    """
    path = body['path']
    filename = body['fname']
    res_str = ""
    (key_based, user, pwd, key_file) = get_cred(body['hostName'], body)
    with produce_ABClusterHost(body['hostName'], key_based, user, pwd, key_file) as ch:
        _logger.debug('RHD--> Produced ABC (cfFER).')
        if path == "~":
            path = ch.homedir
            path = os.path.join(path, '.mcc')
        sp = ch.path_module.join(path, filename)
        _logger.debug('RHD--> cf pathname %s in checking.', sp)
        res_str = ch.file_exists(sp)
        if res_str is not None:
            result = 1
        else:
            result = 0
        _logger.debug('RHD--> cfFER result is %s.', result)
        return  make_rep(req, {
            'host': {'name': ch.host},
            'hostRes': {'fname': filename, 'exists': result}})

def handle_createCfgFileReq(req, body):
    """Handler function for createCfgFileReq command.
    req - top level message object
    body - shortcut to the body part of the message
    Plain file create on host.
    """
    global CONFIGFILE
    path = body['path']
    CONFIGFILE = body['fname']
    (key_based, user, pwd, key_file) = get_cred(body['hostName'], body)
    with produce_ABClusterHost(body['hostName'], key_based, user, pwd, key_file) as ch:
        if path == "~":
            path = ch.homedir
            path = os.path.join(path, '.mcc')
        _logger.debug('RHD--> path is %s and name is %s.', path, CONFIGFILE)
        pathname = ch.path_module.join(path, CONFIGFILE)
        if body.has_key('contentString'):
            with ch.open(pathname, 'w+') as rf:
                rf.write(body['contentString'])
            with ch.open(pathname) as rf:
                assert rf.read() == body['contentString']

        _logger.warning(bcolors.WARNING + 'RHD--> File %s created.' + bcolors.ENDC, pathname)
        return  make_rep(req, {
            'host': {'name': ch.host},
            'hostRes': {'fname': CONFIGFILE, 'created': 1}})

def handle_readCfgFileReq(req, body):
    """Handler function for readCfgFileReq command.
    req - top level message object
    body - shortcut to the body part of the message
    Plain file read on host.
    Body:
        hostName, path, fname
    """
    path = body['path']
    global CONFIGFILE
    CONFIGFILE = body['fname']
    (key_based, user, pwd, key_file) = get_cred(body['hostName'], body)
    with produce_ABClusterHost(body['hostName'], key_based, user, pwd, key_file) as ch:
        _logger.debug('RHD--> Inside produce_ABClusterHost, readcfgfilereq.')
        if path == "~":
            path = ch.homedir
            path = os.path.join(path, '.mcc')
        pathname = ch.path_module.join(path, CONFIGFILE)
        _logger.debug('RHD--> readCfgFileReq, pathname %s in opening.', pathname)
        with open(pathname, 'rb') as ren_cfg:
            res = ren_cfg.read()
            if res.startswith('gAAAAAB'):
                #It's encrypted, get passphrase.
                pass_phr = raw_input("Please provide passphrase for encrypted configuration file " +
                                     str(CONFIGFILE) + ": ")
                res = decrypt(pass_phr, res)
        _logger.debug('RHD--> readCfgFileReq, File %s read.', pathname)
        return  make_rep(req, {
            'host': {'name': ch.host},
            'hostRes': {'fname': CONFIGFILE, 'contentString': res}
        })

def handle_shutdownServerReq():
    """x"""
    raise ShutdownException("Shutdown request received")

def handle_getLogTailReq(req, body):
    """Handler function for getLogTailReq commands. Opens a file on the remote
    host and adds content to reply
    req - top level message object
    body - shortcut to the body part of the message
    """

    sf = body['logFile']
    assert (sf.has_key('path') and sf.has_key('name') and sf.has_key('hostName'))
    (key_based, user, pwd, key_file) = get_cred(sf['hostName'], body)

    with produce_ABClusterHost(sf['hostName'], key_based, user, pwd, key_file) as ch:
        sp = ch.path_module.join(sf['path'], sf['name'])
        assert (ch.file_exists(sp)), 'File ' + sp + ' does not exist on host ' + ch.host
        with ch.open(sp) as log_file:
            return make_rep(req, {'tail': log_file.read()})

def parse_reply(ctx):
    """Return False unless ctx['str'] is an mgmd reply. Assign first line to
    ctx['reply_type], parse property list and return True otherwise."""
    return _parse_until_delim(ctx, 'reply_type', '\n') and parse_properties(ctx, ': ')

class MgmdReply(dict):
    """Parse ndb_mgm reply."""
    def __init__(self, s=None):
        if s:
            ctx = {'str': s, 'properties':self}
            parse_reply(ctx)
            self.reply_type = ctx['reply_type']
    def __str__(self):
        return self.reply_type+'\n'+'\n'.join(['{0}: {1}'.format(\
            str(k), str(self[k])) for k in self.keys()])+'\n'

def handle_SSHCleanupReq(req, body):
    """Handler function for cleaning up permanent SSH connection array."""
    cleanup_connections()
    body = {'out': "OK"}
    return make_rep(req, body)

def handle_copyKeyReq(req, body):
    """Handler function for copying public keys between OPC hosts. Opens connections to
    hosts and sends commands. This version copies id_rsa.pub from 1 host ("master") to all
    other hosts in Cluster. ATM, "master" is just first host in list."""
    #    body: {
    #        ssh: createSSHBlock(mcc.gui.getClSSHUser(), mcc.gui.getClSSHPwd(),
    #            mcc.gui.getClSSHKeyFile()),
    #        hosts: hostlist,
    #        hostsinternal: hostlistinternal, not necessary, code changed.
    # Array to be returned to FE in body.result.contentString. If empty, no problems whatsoever.
    err_arr = []
    # Get values from msg.body.
    _logger.debug('RHD--> ' + "KEEnter running for hostlist " + str(body['hosts']))
    hostlist = body['hosts'].encode('ascii', 'ignore').split(',')
    _logger.debug('RHD--> ' + "KEEnter running preped hostlist " + str(hostlist))

    # Frontend issued connect command for all remote hosts so if we're here, all hosts are conected.
    local_file = os.path.join(os.path.expanduser('~'), ".mcc", "") + "mcckeyex"
    # We execute command locally on that host.
    _logger.debug("RHD--> Preparing key exchange on %s hosts.", len(hostlist))
    # IF there is leftover from previous run, delete.
    if os.path.exists(local_file):
        os.remove(local_file)
    for x in xrange(len(hostlist)):
        _logger.debug("RHD--> uname on %s", hostlist[x])
        with produce_ABClusterHost(hostlist[x], None, None, None, None) as ch:
            if ch.uname == 'Windows':
                # no-go
                hostlist[x] = ''
                _logger.error(bcolors.FAIL +
                              'Not copying SSH keys to remote Windows hosts!' + bcolors.ENDC)
                err_arr.append('Not copying SSH keys to remote Windows hosts!')

    # remove Windows host(s) from list
    hostlist = [x for x in hostlist if x != '']
    if len(hostlist) <= 1:
        err_arr.append('Not copying SSH keys to remotehosts as all are  Windows!')
        return make_rep(req, {'result': {'contentString': err_arr}})

    # all remote hosts are *nix and should understand
    # List of commands we should run successfully on master node before actual copying:
    #   cmd = "cd ~/.ssh"                                                           TERMINAL FAILURE
    #   cmd = "ls -l ~/.ssh/id_rsa.pub"
    #       In case it fails: cmd = 'ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa'      TERMINAL FAILURE
    #   cmd = "ls -l ~/.ssh/id_rsa.pub"                                             TERMINAL FAILURE
    #   cmd = "grep -F -f ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys"
    #       In case it fails: cmd = "cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys"
    #   cmd = "pwd" Not terminal, we'll use "opc" default if this fails.
    #   sftp id_ras.pub from master node locally                                    TERMINAL FAILURE
    #   if os.path.isfile(local_file):                                              TERMINAL FAILURE
    rhn = hostlist[0]
    # 3: ERROR_PATH_NOT_FOUND & 2: ERROR_FILE_NOT_FOUND
    _logger.debug("RHD--> SSH master host is %s", rhn)
    with produce_ABClusterHost(hostlist[0], None, None, None, None) as ch:
        cmd = "cd ~/.ssh"
        _logger.debug("RHD--> cd ~/.ssh on %s", rhn)
        rd = ch.exec_cmdv(cmd, {'noRaise': 1, 'waitForCompletion': True})
        # was there an error?
        if rd.find("errcode:") > -1:
            _logger.warning(bcolors.WARNING + "RHD--> cd ~/.ssh failed on host %s" +
                            bcolors.ENDC, rhn)
            err_arr.append("cd ~/.ssh failed on host " + rhn)
            # ERROR. Existence of ~/.ssh is precondition...
            if os.path.exists(local_file):
                os.remove(local_file)
            _logger.debug("RHD--> Done copying SSH keys.")
            return make_rep(req, {'result': {'contentString': err_arr}})
        else:
            _logger.debug("RHD--> ls -l ~/.ssh/id_rsa.pub on %s", rhn)
            cmd = "ls -l ~/.ssh/id_rsa.pub"
            # 2: No such file or directory.
            rd = ch.exec_cmdv(cmd, {'noRaise': 2, 'waitForCompletion': True})
            # was there an error? if so it's a signal to generate keypair, nothing else.
            _logger.debug("RHD--> ls -l ~/.ssh/id_rsa.pub done on %s", rhn)
            if rd.find("errcode:") > -1:
                # Generate key pair.
                # '...""...' works on Windows too while "...''..." does not.
                cmd = 'ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa'
                _logger.debug("RHD--> %s on %s", cmd, rhn)
                # 1: No such file or directory. Option error is 255.
                rd = ch.exec_cmdv(cmd, {'noRaise': 1, 'waitForCompletion': True})
                _logger.debug('RHD--> %s done on %s', cmd, rhn)
                if rd.find("errcode:") > -1:
                    log_msg = "RHD--> SSH keygen failed on host %s"
                    _logger.warning(bcolors.WARNING + log_msg + bcolors.ENDC, rhn)
                    err_arr.append("SSH keygen failed on host " + rhn)
                    # ERROR. No keys to copy.
                    if os.path.exists(local_file):
                        os.remove(local_file)
                    _logger.debug("RHD--> Done copying SSH keys.")
                    return make_rep(req, {'result': {'contentString': err_arr}})

            cmd = "ls -l ~/.ssh/id_rsa.pub"
            _logger.debug("RHD--> %s on %s", cmd, rhn)
            rd = ch.exec_cmdv(cmd, {'noRaise': 2, 'waitForCompletion': True})
            # was there an error? if so it's a signal to generate keypair, nothing else.
            _logger.debug("RHD--> %s done on %s", cmd, rhn)
            if rd.find("errcode:") > -1:
                _logger.warning(bcolors.WARNING +
                                "RHD--> %s failed on host %s." + bcolors.ENDC, cmd, rhn)
                err_arr.append(cmd + " on host " + rhn + ' failed.')
                if os.path.exists(local_file):
                    os.remove(local_file)
                _logger.debug("RHD--> Done copying SSH keys.")
                return make_rep(req, {'result': {'contentString': err_arr}})
            # we have ~/.ssh/id_rsa.pub
            cmd = "/usr/bin/grep -F -f ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys"
            _logger.debug("RHD--> %s %s", cmd, rhn)
            # grep will return 1 if !found, 0 otherwise.
            rd = ch.exec_cmdv(cmd, {'noRaise': 1, 'waitForCompletion': True})
            # was there an error? if so it's a signal to add the key, nothing else.
            _logger.debug("RHD--> %s done on %s", cmd, rhn)
            if rd.find("errcode:") > -1:
                # 0 means add key to list
                cmd = "/usr/bin/cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys"
                _logger.debug("RHD--> %s %s", cmd, rhn)
                rd = ch.exec_cmdv(cmd, {'noRaise': 1, 'waitForCompletion': True})
                # was there an error? if so, we have nothing to copy....
                if rd.find("errcode:") > -1:
                    _logger.warning(bcolors.WARNING +
                                    "RHD--> %s failed on host %s."
                                    + bcolors.ENDC, cmd, rhn)
                    err_arr.append(cmd + " failed on host " + rhn)

            # We need PWD here to pass full path to SFTP.
            cmd = "pwd"
            _logger.debug("RHD--> pwd on %s", rhn)
            rd = ch.exec_cmdv(cmd)
            _logger.debug("RHD--> pwd done on %s", rhn)
            if rd.find("errcode:") > -1:
                log_msg = "RHD--> pwd failed on host %s. Trying to recover using " + \
                    "OPC defaults."
                _logger.warning(bcolors.WARNING + log_msg + bcolors.ENDC, rhn)
                err_arr.append("PWD failed on host " + rhn +
                               ". Trying to recover using OPC defaults.")
                s = "/home/opc"
            else:
                s = rd.strip()
            s = s + "/.ssh/id_rsa.pub"
            log_msg = "RHD--> On host %s. Trying to sftp %s ->> %s."
            _logger.debug(log_msg, rhn, s, local_file)
            # get the key from master node to local machine:
            ch.get(s, local_file)
            _logger.debug("RHD--> Done trying to sftp %s on %s", s, rhn)
            # check key arrived
            if not os.path.isfile(local_file):
                log_msg = "RHD--> Couldn't SFTP key from master node to localhost!"
                _logger.warning(bcolors.WARNING + log_msg + bcolors.ENDC, rhn)
                err_arr.append("SFTP from master node to localhost failed!")
                # ERROR. No keys to copy.
                if os.path.exists(local_file):
                    os.remove(local_file)
                _logger.debug("RHD--> Done copying SSH keys.")
                return make_rep(req, {'result': {'contentString': err_arr}})

    # Now we loop from 1 to len(hostlist) [0 is master node] and copy local file to hosts +
    # add it to authorized_hosts. Failure to add key from master node to any of the hosts is not
    # considered terminal. We just move on to next host in list.
    #
    # Go on to inner FOR loop, distribute the key around.
    _logger.debug("RHD--> Distributing key over hosts.")
    for y in xrange(len(hostlist)):
        if y == 0: # Skip master node.
            _logger.debug("RHD--> Skipping over node %s.", hostlist[0])
            continue
        with produce_ABClusterHost(hostlist[y], None, None, None, None) as ch:
            rhn_y = str(hostlist[y])
            _logger.debug("RHD--> Distributing key over hosts, host %s", y)
            cmd = "cd ~/.ssh"
            _logger.debug("RHD--> Running %s on host indexed %s", cmd, y)
            rd = ch.exec_cmdv(cmd, {'noRaise': 1, 'waitForCompletion': True})
            _logger.debug("RHD--> %s done on %s", cmd, rhn)
            if rd.find("errcode:") > -1:
                wrn_msg = "RHD--> %s failed on host %s"
                _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, cmd, rhn_y)
                err_arr.append(cmd + " failed on host " + rhn_y)
            else:
                # We need PWD here to pass full path to SFTP.
                cmd = "pwd"
                _logger.debug("RHD--> Running %s on host indexed %s", cmd, y)
                rd = ch.exec_cmdv(cmd)
                _logger.debug("RHD--> %s done on %s", cmd, rhn)
                if rd.find("errcode:") > -1:
                    wrn_msg = "RHD--> %s failed on host %s"
                    _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, cmd, rhn_y)
                    err_arr.append(cmd + " failed on host " + rhn_y)
                    s = "/home/opc"
                else:
                    s = rd.strip()
                s = s + "/.ssh/mcckeyex"
                log_msg = "RHD--> From host %s. Trying to sftp mcckeyex(" + \
                    "local) to %s::%s"
                _logger.debug(log_msg, rhn, rhn_y, s)
                # there might be mcckeyex file already there from some failed run.
                cmd = "ls -l ~/.ssh/mcckeyex"
                _logger.debug("RHD--> Running %s on host indexed %s", cmd, y)
                rd = ch.exec_cmdv(cmd, {'noRaise': 2, 'waitForCompletion': True})
                _logger.debug("RHD--> %s done on %s", cmd, rhn_y)
                if rd.find("errcode:") <= -1:
                    # there is leftover key exchange file, remove
                    cmd = "rm ~/.ssh/mcckeyex"
                    _logger.debug("RHD--> Running %s on host indexed %s", cmd, y)
                    rd = ch.exec_cmdv(cmd, {'noRaise': 2, 'waitForCompletion': True})
                    _logger.debug("RHD--> %s done on %s with %s", cmd, rhn_y, str(rd))

                # upload key file and check it's there
                ch.put(local_file, s)
                cmd = "ls -l ~/.ssh/mcckeyex"
                _logger.debug("RHD--> Running %s on host indexed %s", cmd, y)
                rd = ch.exec_cmdv(cmd, {'noRaise': 2, 'waitForCompletion': True})
                _logger.debug("RHD--> %s done on %s", cmd, rhn_y)
                if rd.find("errcode:") > -1:
                    wrn_msg = "RHD--> %s failed on host %s"
                    _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, cmd, rhn_y)
                    err_arr.append(cmd + " failed on host " + rhn_y)
                else:
                    cmd = "/usr/bin/grep -F -f ~/.ssh/mcckeyex ~/.ssh/authorized_keys"
                    _logger.debug("RHD--> Running %s on host indexed %s", cmd, y)
                    rd = ch.exec_cmdv(cmd, {'noRaise': 1, 'waitForCompletion': True})
                    _logger.debug("RHD--> %s done on %s", cmd, rhn_y)
                    # grep will return 1 if !found, 0 otherwise.
                    if rd.find("errcode:") > -1:
                        # Add the key
                        cmd = "/usr/bin/cat ~/.ssh/mcckeyex >> ~/.ssh/authorized_keys"
                        _logger.debug("RHD--> Running %s on host indexed %s", cmd, y)
                        rd = ch.exec_cmdv(cmd, {'noRaise': 1, 'waitForCompletion': True})
                        _logger.debug("RHD--> %s done on %s", cmd, rhn_y)
                        if rd.find("errcode:") > -1:
                            wrn_msg = "RHD--> %s failed on host %s"
                            _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, cmd, rhn_y)
                            err_arr.append(cmd + " failed on host " + rhn_y)

                    cmd = "rm ~/.ssh/mcckeyex"
                    _logger.debug("RHD--> Running %s on host indexed %s", cmd, y)
                    rd = ch.exec_cmdv(cmd, {'noRaise': 1, 'waitForCompletion': True})
                    _logger.debug("RHD--> %s done on %s", cmd, rhn_y)
                    if rd.find("errcode:") > -1:
                        wrn_msg = "RHD--> %s failed on host %s"
                        _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC, cmd, rhn_y)
                        err_arr.append(cmd + " failed on host " + rhn_y)

    # Outer FOR loop finished, do cleanup.
    if os.path.exists(local_file):
        os.remove(local_file)
    _logger.debug("RHD--> Done copying SSH keys.")
    # Figure out how and what to return to front end... readcfgfilereq
    return make_rep(req, {'result': {'contentString': err_arr}})

def handle_getLogsReq(req, body):
    """Handler function for getting log files locally (~/.mcc). Opens connections
    to hosts and gets log files.
    body {
        sourceFile: { remote
            hostName: hostname,
            path: srcPath,
            name: srcName
        },
        destinationFile: { local
            name: destName
        }
    }
    """
    hostname = body['sourceFile']['hostName'].encode('ascii', 'ignore')
    (key_based, user, pwd, key_file) = get_cred(hostname, body)
    with produce_ABClusterHost(hostname, key_based, user, pwd, key_file) as ch:
        local_file = os.path.join(os.path.expanduser('~'), ".mcc", "")
        local_file = os.path.join(local_file, body['destinationFile']['name'])
        _logger.debug("RHD--> local file is %s", local_file)
        remote_file = os.path.join(body['sourceFile']['path'], body['sourceFile']['name'])
        _logger.debug("RHD--> remote file is %s", remote_file)
        res = ch.get(remote_file, local_file)
        _logger.debug("RHD--> res is %s", res)
        return make_rep(req, {'out': res})

def handle_runMgmdCommandReq(req, body):
    """Handler function for runMgmdCommandReq commands. Opens a new connection to mgmd,
    sends command, parses reply and wraps reply in mcc Rep object."""
    global MGMT_CONN
    global CTRLCREC
    if CTRLCREC:
        return make_rep(req, {'reply_type': 'ERROR'})

    conn_idx = -2 # Init to error value.
    # Get values from msg.body.
    hostname = body['hostName'].encode('ascii', 'ignore')
    port = body['port']
    cmd = body['mgmd_command'].encode('ascii', 'ignore')
    inst_dir = body['inst_dir'].encode('ascii', 'ignore') # Where to look for ndb_mgm
    os_name = body['uname'].encode('ascii', 'ignore')

    (key_based, user, pwd, key_file) = get_cred(hostname, body)
    # Sanitize:
    if not (user and user.strip()):
        user = None
    if not (pwd and pwd.strip()):
        pwd = None
    else:
        if pwd.startswith('**'):
            pwd = None
    if not (key_file and key_file.strip()):
        key_file = None

    deploy_page_exit = cmd
    if deploy_page_exit.find("STOP ") == 0:
        # FE user left DEPLOYMENT page, it is always possible user will change
        # Cluster configuration thus we need to clear MGMT conn array.
        wrn_msg = "RHD--> STOP request, cleaning up MGMTconn array."
        _logger.warning(bcolors.WARNING + wrn_msg + bcolors.ENDC)
        # There is STOP command.
        cleanup_mgmt_connections()
        # Reform into valid command.
        cmd = deploy_page_exit.split("STOP ")[1]
        # "Fake" real status call and stop polling.
        conn_idx = -1
    else:
        # Get index of connection to use.
        conn_idx = setup_mgmt_conn(hostname, key_based, user, pwd, key_file, inst_dir, os_name)
        _logger.debug("RHD--> MGMTConn connIdx is %s for host %s", conn_idx, hostname)
    # We have body.ssh.KeyBased here to determine if we should tunnel through SSH.
    # If HostIsLocal, no need for tunneling too (is_host_local(HostDesignation)).
    # connIdx = -1, GOTO old code.

    if conn_idx == -2:
        # Error condition!
        _logger.error(bcolors.FAIL + "RHD--> Error before connecting!" + bcolors.ENDC)
        return make_rep(req, {'reply_type': 'OK'})

    # Use old code for LOCALHOST/STOP
    if conn_idx == -1:
        _logger.debug("RHD--> connIdx is -1")
        if deploy_page_exit.find("STOP ") == 0:
            _logger.debug("RHD--> STOP found, returning")
            return make_rep(req, {'reply_type': 'ERROR'})

        # If none is listening, this will lead to Error 10061 (Can't connect to MySQL server)
        # so need to test first.
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        result = s.connect_ex((hostname, int(port)))
        # _logger.debug("RHD--> result from socket is %s", result)
        s.close()

        if result == 0:
            _logger.debug("RHD--> Trying socket shutter (0)")
            with util.socket_shutter(socket.create_connection((hostname, int(port)))) as mgmd:
                try:
                    mgmd.sendall(cmd+'\n\n') # body['mgmd_command']+'\n\n')
                    s = mgmd.recv(4096) # s is multiline string.
                    # too big output, enable only in case of trouble
                    # _logger.debug("RHD--> result from socket sendall is %s", s)
                except Exception:
                    _logger.error(bcolors.FAIL + "RHD--> Exception in socket shutter, no " +
                                  "management response." + bcolors.ENDC)
                    return make_rep(req, {'reply_type': 'ERROR', 'reply_properties': {
                        '49':
                            {
                                'status': 'UNKNOWN',
                                'version': 'UNKNOWN',
                                'mysql_version': 'UNKNOWN',
                                'address': 'UNKNOWN',
                                'connect_count': 'UNKNOWN',
                                'dynamic_id': 'UNKNOWN',
                                'startphase': 'UNKNOWN',
                                'type': 'MGM',
                                'node_group': '0'
                            }
                    }})

            status = MgmdReply(s)
            sd = {}
            for nk in status.keys():
                if 'node.' in nk:
                    (x, n, k) = nk.split('.')
                    if not sd.has_key(n):
                        sd[n] = {}
                    sd[n][k] = status[nk]
            return make_rep(req, {'reply_type': status.reply_type, 'reply_properties':sd})
        _logger.error(bcolors.FAIL + "RHD--> result from socket was error" + bcolors.ENDC)
        return make_rep(req, {'reply_type': 'ERROR'})

    if MGMT_CONN[conn_idx] is None:
        _logger.debug("RHD--> Clean up and Reconnect")
        cleanup_mgmt_connections()
        conn_idx = setup_mgmt_conn(hostname, key_based, user, pwd, key_file, inst_dir, os_name)
        _logger.debug("RHD--> MGMTConn connIdx is %s for host %s", conn_idx, hostname)
    else:
        # Connection validity check.
        if not check_connected(MGMT_CONN[conn_idx]):
            _logger.debug("RHD--> Clean up and Reconnect")
            cleanup_mgmt_connections()
            conn_idx = setup_mgmt_conn(hostname, key_based, user, pwd, key_file, inst_dir, os_name)
            _logger.debug("RHD--> MGMTConn connIdx is %s for host %s", conn_idx, hostname)

    # We have request to send to remote host. However, we execute command locally on that host.
    # _logger.debug("RHD--> Inside new MGMT req. handling code.")
    cmd = MGMT_CONN[conn_idx].fullCmd + " localhost:" + str(port) + \
        ' -e"SHOW" --connect-retries=1\n' # Avoid 1min of retries.
    stdin_, stdout_, stderr_ = MGMT_CONN[conn_idx].exec_command(cmd, 8192)
    # To be able to throw error from here.
    res = stdout_.channel.recv_exit_status()
    s = stdout_.readlines()
    # err = stderr_.readlines()
    if res > 0:
        return make_rep(req, {'reply_type': 'ERROR'})
    if len(s) >= 1:
        if len(s[0]) > 5:
            if s[0].encode('ascii', 'ignore').find('Unable') == 0:
                return make_rep(req, {'reply_type': 'ERROR'})

    # Parse and format output.
    z = 0
    st = ""
    sta = ""
    statt = ""
    section = ""

    # check new boundaries
    for i in xrange(len(s)):
        if s[i].encode('ascii', 'ignore').find('id=') >= 0:
            z += 1
    # Z is total # of nodes in Cluster which is required in output header.
    for i in xrange(len(s)):
        # Is it section?
        if s[i].encode('ascii', 'ignore').find(')]') >= 0:
            pos = s[i].encode('ascii', 'ignore').find(')]')
            section = s[i].encode('ascii', 'ignore')[pos-3:pos]
        # Not enough space to write this so using lot's of continue...
        # Is it NODE?
        if not section or s[i].encode('ascii', 'ignore').find('id=') < 0:
            continue # Nope.

        # Form the answer:
        st = s[i].encode('ascii', 'ignore').replace("\t", " ")
        sta = st.replace("id=", "node.").split(" ")[0]
        statt += sta + ".type: " + section + "\n"
        if st.find("@") < 4:
            statt += sta + ".status: NO_CONTACT\n"
            continue
        if section != "NDB":
            statt += sta + ".status: CONNECTED\n"
            continue

        if len(st.split("(")[1].split(",")) < 3:
            statt += sta + ".status: STARTED\n"
            continue

        if st.split("(")[1].split(",")[1].strip().find("Nodegro") < 0:
            # STARTED CONNECTED UNKNOWN?
            statt += sta + ".status: " + st.split("(")[1].split(",")[1].strip().replace(\
                "not connected", "NO_CONTACT").replace("not started", \
                    "NOT_STARTED").replace("starting", "STARTING").replace("started", "STARTED").\
                        replace("shutting down", "SHUTTING_DOWN").replace("restarting", \
                            "RESTARTING") + "\n"
        else:
            statt += sta + ".status: STARTED\n"

    # Ad header lines to result.
    statt = "node status\nnodes: " + str(z) + "\n" + statt
    # Now, parse stat to get info.
    status = MgmdReply(statt)
    sd = {}
    for nk in status.keys():
        if 'node.' in nk:
            (x, n, k) = nk.split('.')
            if not sd.has_key(n):
                sd[n] = {}
            sd[n][k] = status[nk]
    return make_rep(req, {'reply_type': status.reply_type, 'reply_properties':sd})

def handle_getConfigIni(req, body):
    """Unused"""
    cf = body['configFile']
    assert (cf.has_key('path') and cf.has_key('name') and cf.has_key('hostName'))
    (key_based, user, pwd, key_file) = get_cred(cf['hostName'], body)

    with produce_ABClusterHost(cf['hostName'], key_based, user, pwd, key_file) as ch:
        sp = ch.path_module.join(cf['path'], cf['name'])
        assert (ch.file_exists(sp)), 'File ' + sp + ' does not exist on host ' + ch.host
        with ch.open(sp) as ini:
            return make_rep(req, {'config': config_parser.parse_cluster_config_ini_(ini)})

def handle_getNdbConfig(req, body):
    """Unused"""
    (key_based, user, pwd, key_file) = get_cred(body['hostName'], body)

    with produce_ABClusterHost(body['hostName'], key_based, user, pwd, key_file) as ch:
        ndb_config = ch.path_module.join(body['installpath'], 'ndb_config')
        return make_rep(req, {
            'ndb_config': json.dumps(
                util.xml_to_python(ch.exec_cmdv([ndb_config, '--configinfo', '--xml']))
            )
        })

def log_thread_name():
    """Utility for dumping thread id in the log."""
    cur_thread = threading.current_thread()
    _logger.debug("RHD--> cur_thread=%s", cur_thread.name)

class ConfiguratorHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    """Handler class for web server requests to the configurator backend. To be used with
    BaseHTTPServer.HTTPServer to create a working server."""

    def _send_as_json(self, obj):
        self.send_response(200)
        self.send_header('Content-type', 'text/json')
        self.end_headers()

        # One notable difference in Python 2 is that if you're using ensure_ascii=False,
        # dump will properly write UTF-8 encoded data into the file (unless you used
        # 8-bit strings with extended characters that are not UTF-8):
        # dumps on the other hand, with ensure_ascii=False can produce a str or
        # Unicode just depending on what types you used for strings:
        #     Serialize obj to a JSON formatted str using this conversion table. If
        #     ensure_ascii is False, the result may contain non-ASCII characters and
        #     the return value may be a Unicode instance.

        # Note that it may still be a str instance as well.
        # Thus you cannot use its return value without checking which format was
        # returned and possibly playing with Unicode.encode.
        # This of course is not valid concern in Python 3 any more, since there is
        # no more this 8-bit/Unicode confusion.

        val_err = False
        possible_encoding = "utf-8"
        for possible_encoding in ["utf-8", "cp1252"]:
            try:
                # obj can contain messages from the OS that could be encoded in cp1252 charset.
                json_str = json.dumps(obj, indent=2, cls=ReplyJsonEncoder,
                                      encoding=possible_encoding)
                break
            except UnicodeDecodeError:
                wrn_msg = 'RHD--> %s encoding failed'
                self.server.logger.debug(wrn_msg, possible_encoding)
                wrn_msg = bcolors.FAIL + wrn_msg + bcolors.ENDC
                self.server.logger.warning(wrn_msg, possible_encoding)
            except ValueError:
                # There is no problem with encoding but rather some value conversion error. Assuming
                # we need to convert tuples, say ('HOST',port) tuple, say ('129.146.115.208', 22),
                # into 'HOST:port' string for this to work.
                val_err = True
                obj1 = dict((':'.join(k), v) for k, v in obj.items())
                json_str = json.dumps(obj1, indent=2, cls=ReplyJsonEncoder,
                                      encoding=possible_encoding)
                break

        if json_str is None and not val_err:
            raise UnicodeDecodeError
        # Enable in case of problems
        # self.server.logger.debug('RHD--> Will send: %s', json.dumps(obj, indent=2, \
        #    cls=ReplyJsonEncoder, encoding=possible_encoding))
        json.dump(obj, self.wfile, cls=ReplyJsonEncoder, encoding=possible_encoding)

    def _do_file_req(self, rt):
        """Handles file requests. Attempts to guess
        the MIME type and set the Content-Type accordingly."""
        try:
            # This is suffocating console, enable only in dire emergency situation.
            # log_thread_name()
            if self.path == '/':
                self.path = '/'+mcc_config.MCC_BROWSER_START_PAGE
            # self.server.logger.debug("RHD--> " + rt+' fdir='+self.server.opts['fdir'] +
            # " path="+os.path.normpath(self.path))

            # Make sure to remove options if there are any.
            fn = os.path.join(self.server.opts['fdir'], os.path.normpath(
                self.path[1:]).split("?")[0])
            try:
                os.stat(fn)
            except OSError as ose:
                log_thread_name()
                err_msg = bcolors.FAIL + 'RHD--> %s %s failed' + bcolors.ENDC
                self.server.logger.exception(err_msg, rt, self.path)
                if ose.errno == errno.ENOENT:
                    self.send_error(404, self.path+'=> file://'+ose.filename + ' does not exist')
                    return
            self.send_response(200)
            (ct, enc) = mimetypes.guess_type(self.path)
            if ct:
                self.send_header('Content-type', ct)
            if enc:
                self.send_header('Content-Encoding', enc)
            self.end_headers()
            if rt == 'GET':
                with open(fn, "rb") as f:
                    self.wfile.write(f.read()+'\r\n\r\n')
        except:
            log_thread_name()
            err_msg = bcolors.FAIL + 'RHD--> %s %s failed' + bcolors.ENDC
            self.server.logger.exception(err_msg, rt, self.path)
            self.send_error(500, 'Unexpected exception occured while processing: ' +
                            rt+' '+self.path+'\n'+traceback.format_exc()) # Some other number

    def do_HEAD(self):
        """Handles HEAD requests by returning the headers without the body if file can be stated."""
        self._do_file_req('HEAD')

    def do_GET(self):
        """Handles GET requests by returning the specified file."""
        self._do_file_req('GET')

    def do_POST(self):
        """Handles json-serialized command object from the frontend through POST requests."""
        log_thread_name()
        try:
            # Assume all post requests are json
            assert  'application/json' in self.headers['Content-Type']
            msg = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            dbg_msg = copy.deepcopy(msg)
            if dbg_msg['body']['ssh'].has_key('pwd'):
                dbg_msg['body']['ssh']['pwd'] = '*' * len(dbg_msg['body']['ssh']['pwd'])
            if dbg_msg['body']['ssh'].has_key('key_passp'):
                dbg_msg['body']['ssh']['key_passp'] = '*' * len(dbg_msg['body']['ssh']['key_passp'])
            self.server.logger.debug('RHD--> %s:%i', dbg_msg['head']['cmd'], dbg_msg['head']['seq'])
            # This is too much... Enable only in case of trouble.
            # self.server.logger.debug("RHD--> " + pprint.pformat(dbgmsg))
            rep = make_rep(msg)
            try:
                rep = handle_req(msg)
            except ShutdownException:
                self.server.shutdown()
            except:
                err_msg = bcolors.FAIL + 'RHD--> POST request failed:' + bcolors.ENDC
                self.server.logger.exception(err_msg)
                (e_type, e_obj, e_tb) = sys.exc_info()
                rep['stat'] = {
                    'errMsg': util.html_rep(e_obj),
                    'exType': e_type,
                    'exObj': e_obj,
                    'exTrace': e_tb
                }
            self.server.logger.debug('RHD--> ' + rep['head']['cmd'] + ':')
            # Enable in case of problems.
            # self.server.logger.debug('RHD--> ' + pprint.pformat(rep))
            self._send_as_json(rep)
        except:
            cr_msg = bcolors.CRITICAL + 'RHD--> Internal server error:' + bcolors.ENDC
            self.server.logger.critical(cr_msg)
            cr_msg = bcolors.FAIL + 'RHD--> Unexpected exception:' + bcolors.ENDC
            self.server.logger.exception(cr_msg)
            self.send_error(500, 'Server Exception\n'+traceback.format_exc() +
                            'while processing request:\n'+str(self.headers))

    def log_message(self, fmt, *args):
        """Overrides the default implementation which logs to stderr"""
        # Do not log GET of DOJO and images, it's just too much...
        m = args[0]
        try:
            if 'GET /dojo' not in m and 'GET /img' not in m:
                self.server.logger.info(msg=(fmt%args))
        except TypeError:
            # Not iterable, just go with it.
            self.server.logger.info(msg=(fmt%args))

class ConfiguratorServer(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
    """Specialization of HTTPServer which adds ssl wrapping, shutdown on close and
    MT (by also inheriting SocketServer.ThreadingMixIn."""
    def __init__(self, opts):
        # Cannot use super() here, as BaseServer is not a new-style class
        SocketServer.TCPServer.__init__(self, (opts['server'], opts['port']), ConfiguratorHandler)
        self.opts = opts
        self.logger = logging.getLogger(str(self.__class__))

    def get_request(self):
        """Override get_request from SocketServer.TCPServer so that we can wrap the socket in an ssl
        socket when using ssl."""
        sock, addr = SocketServer.TCPServer.get_request(self)
        if util.get_val(self.opts, 'ssl', False):
            if self.opts['ca_certs']:
                cert_reqs = ssl.CERT_REQUIRED
            else:
                cert_reqs = ssl.CERT_NONE

            ssl_sock = ssl.wrap_socket(sock, certfile=self.opts['certfile'],
                                       keyfile=self.opts['keyfile'], cert_reqs=cert_reqs,
                                       ca_certs=self.opts['ca_certs'], server_side=True)
            #self.logger.debug('RHD--> ssl_sock.getpeercert()='+str(ssl_sock.getpeercert())
            return ssl_sock, addr
        return sock, addr

    def close_request(self, request):
        """Override to make sure shutdown gets called on request socket."""
        try:
            request.shutdown(socket.SHUT_RDWR)
        except:
            pass
        finally:
            SocketServer.TCPServer.close_request(self, request)

CONFIGDIR = None
BASEDIR = None

def main(prefix, cfg_dir):
    """Server's main-function which parses the command line and starts up the server accordingly.
    """
    global CONFIGDIR
    global BASEDIR
    global PASSPHRASE
    global CTRLCREC

    true_port_val = 0
    CONFIGDIR = cfg_dir
    BASEDIR = prefix
    def_server_name = 'localhost'
    if hasattr(webbrowser, 'WindowsDefault') and \
        isinstance(webbrowser.get(), webbrowser.WindowsDefault):
        # If on windows and using IE we do this to avoid IE9 bug (doesn't affect other versions of
        # IE, there is no easy way to test).
        def_server_name = socket.gethostname()

    os.system('color')

    cmdln_parser = optparse.OptionParser()
    cmdln_parser.add_option('-N', '--server_name', action='store', type='string', \
        default=def_server_name, help='server name: [default: %default ]')
    cmdln_parser.add_option('-p', '--port', \
        action='store', type='int', dest='port', default=8081, \
        help='port for the webserver: [default: %default]')
    cmdln_parser.add_option('-n', '--no-browser', action='store_true',
                            help='do not open the server\'s start page in a browser.')
    cmdln_parser.add_option('-s', '--browser-start-page', action='store', type='string', \
        dest='browser_start_page', default=mcc_config.MCC_BROWSER_START_PAGE, help='start ' + \
            'page for browser: [default: %default]')
    cmdln_parser.add_option('-d', '--debug-level', action='store', type='string', \
        default='WARNING', help='Python logging module debug level (DEBUG, INFO,' \
            ' WARNING, ERROR or CRITICAL). [default: %default]')
    cmdln_parser.add_option('-o', '--server-log-file', action='store', type='string', \
        default=os.path.join(tempfile.gettempdir(), 'ndb_setup-'+str(os.getpid())+'.log'), \
        help='log requests to this file. The value - means log to stderr: [default: %default]')
    # option for level/log_cfg file
    cmdln_parser.add_option('-H', '--use-http', action='store_true', \
        help='use http for communication with browser.')
    cmdln_parser.add_option('-S', '--use-https', action='store_true', \
        help='use https to secure communication with browser.')
    cmdln_parser.add_option('-c', '--cert-file', action='store', type='string', \
        default=os.path.join(cfg_dir, 'cfg.pem'), help='file containing X509 certificate which ' + \
            'identifies the server (possibly self-signed): [default: %default]')
    cmdln_parser.add_option('-k', '--key-file', action='store', type='string', \
        help='file containing private key when if not included in cert-file: [default: %default]')
    cmdln_parser.add_option('-a', '--ca-certs-file', action='store', type='string', \
        help='file containing list of client certificates allowed to connect to the server ' + \
            '[default: %default (no client authentication)]')

    (options, arguments) = cmdln_parser.parse_args()

    dbglvl = getattr(logging, options.debug_level, logging.DEBUG)
    fmt = '%(asctime)s: %(levelname)s [%(funcName)s;%(filename)s:%(lineno)d]: %(message)s '
    if options.server_log_file == '-':
        logging.basicConfig(level=dbglvl, format=fmt)
    else:
        logging.basicConfig(level=dbglvl, format=fmt, filename=options.server_log_file)

    for port_val in xrange(options.port, options.port + 20):
        if is_port_available(options.server_name, port_val):
            true_port_val = port_val
            break
    if true_port_val == 0:
        # no available port in range :-/
        print(bcolors.CRITICAL + "No available port in range[{},{}]!" + bcolors.ENDC.format(\
            options.port, options.port + 20))
        sys.exit()

    options.port = true_port_val

    srv_opts = {
        'server' : options.server_name,
        'port': options.port,
        'cdir': cfg_dir,
        'fdir': os.path.join(cfg_dir, 'frontend')
    }

    if not options.use_https:
        options.use_http = True

    if options.use_https:
        srv_opts['ssl'] = True
        srv_opts['certfile'] = options.cert_file
        srv_opts['keyfile'] = options.key_file
        srv_opts['ca_certs'] = options.ca_certs_file

    path = os.path.expanduser('~')
    path = os.path.join(path, '')
    directory = os.path.join(path, '.mcc')
    if not os.path.exists(directory):
        # There is no ~/.mcc so create and move configuration files there.
        print(bcolors.WARNING + "Attempting to create ~/.mcc and move configurations there." +
              bcolors.ENDC)
        os.makedirs(directory)
        tmp_list = os.listdir(path)
        for fname in tmp_list:
            if (fname.endswith('.mcc') and os.path.getsize(path+fname) > 10):
                if os.path.isfile(os.path.join(path, fname)):
                    os.rename(os.path.join(path, fname), os.path.join(directory, fname))

    flags = os.O_CREAT | os.O_EXCL | os.O_WRONLY
    hpath = os.path.join(directory, '')
    global PID_FILE
    PID_FILE = os.path.join(hpath, 'mcc.pid')
    try:
        file_handle = os.open(PID_FILE, flags)
    except OSError as e:
        if e.errno == errno.EEXIST:  # Failed as the file already exists.
            file_handle = open(PID_FILE) # , os.O_RDONLY)
            print(bcolors.CRITICAL + 'mcc.pid file found at '+os.path.realpath(file_handle.name) +
                  '. Please remove before restarting.' + bcolors.ENDC)
            file_handle.close()
            sys.exit("Web server already running!")
        else:  # Something unexpected went wrong so reraise the exception.
            raise
    else:  # No exception, so the file must have been created successfully.
        with os.fdopen(file_handle, 'w') as file_obj:
            # Using `os.fdopen` converts the handle to an object that acts like a
            # regular Python file object, and the `with` context manager means the
            # file will be automatically closed when we're done with it.
            file_obj.write("MCC running.")
            file_obj.close()

    print(bcolors.OKGREEN + 'Starting web server on port ' + repr(options.port) + bcolors.ENDC)
    url_host = options.server_name
    if url_host == '':
        url_host = 'localhost'
    # Here we're' sending list of config files too.
    results = []
    res_str = ""
    use_same_pp = False
    for cfg_file in os.listdir(hpath):
        if cfg_file.endswith(".mcc"):
            fileandpath = os.path.join(hpath, cfg_file)
            results.append(cfg_file.split(".mcc")[0])
            # Decrypt here
            with open(fileandpath, 'rb') as ren_cfg:
                inp = ren_cfg.read()
                if inp.startswith('gAAAAAB'):
                    if not use_same_pp and PASSPHRASE is None:
                        r = raw_input(bcolors.WARNING + "Use SAME passphrase to decrypt ALL "
                                      "configuration files [y/n]? " + bcolors.ENDC).lower()
                        use_same_pp = bool(r == 'y')
                    # It's encrypted, get passphrase.
                    res = ""
                    if not use_same_pp or PASSPHRASE is None:
                        PASSPHRASE = raw_input(bcolors.WARNING + "Please provide passphrase for "
                                               "encrypted file " + str(cfg_file) + ": " +
                                               bcolors.ENDC)
                    with open(fileandpath, 'wb') as out_file:
                        try:
                            res = decrypt(str(PASSPHRASE).strip(), inp)
                        except cryptography.fernet.InvalidToken:
                            print(bcolors.CRITICAL + "Invalid passphrase " + PASSPHRASE +
                                  '!\nConfiguration file: ' + cfg_file + ' is not usable.' +
                                  bcolors.ENDC)
                            PASSPHRASE = ""
                            res = inp
                        finally:
                            if res != inp:
                                print(bcolors.WARNING + "writing cfg file" + bcolors.ENDC)
                            else:
                                print(bcolors.WARNING + "file still encrypted" + bcolors.ENDC)
                            out_file.write(res)

    res_str = '&'.join(results)
    # Check there is anything.
    if results:
        res_str = "?" + res_str + "#"
    else:
        res_str = ""

    if options.use_https:
        if not res_str:
            url = 'https://{0}:{opt.port}/{opt.browser_start_page}'.format(url_host, opt=options)
        else:
            url = 'https://{0}:{opt.port}/{opt.browser_start_page}{1}'.format(\
                url_host, res_str, opt=options)
    else:
        if not res_str:
            url = 'http://{0}:{opt.port}/{opt.browser_start_page}'.format(url_host, opt=options)
        else:
            url = 'http://{0}:{opt.port}/{opt.browser_start_page}{1}'.format(\
                url_host, res_str, opt=options)
    httpsrv = None
    print(bcolors.FAIL + 'Press CTRL+C to stop web server.' + bcolors.ENDC)
    try:
        httpsrv = ConfiguratorServer(srv_opts)
        if not options.no_browser:
            try:
                webbrowser.open_new(url)
            except:
                logging.exception('Failed to control browser: ')
                print(bcolors.FAIL + 'Could not control your browser. Try to opening ' + url +
                      ' to launch the application.' + bcolors.ENDC)
            else:
                print(bcolors.WARNING + 'The application should now be running in your browser.\n'
                      '(Alternatively you can navigate to '+url+' to start it)' + bcolors.ENDC)
        else:
            print(bcolors.WARNING + 'Navigate to '+url+' to launch the application.' + bcolors.ENDC)

        httpsrv.serve_forever()
    except KeyboardInterrupt:
        print(bcolors.FAIL + '^C received, shutting down web server' + bcolors.ENDC)
    except:
        traceback.print_exc()
    finally:
        # Clean MGMT connections up.
        CTRLCREC = True

        cleanup_mgmt_connections()
        cleanup_connections()
        if httpsrv:
            httpsrv.socket.close()
        print(bcolors.WARNING + 'Removing ' + PID_FILE + bcolors.ENDC)
        os.remove(PID_FILE)
