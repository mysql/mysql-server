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
import pprint
import sys
import types
import ssl
import copy
import socket
import time
import os
import os.path
import logging
import optparse
import webbrowser
import zipfile
import tempfile
import threading
import random
import stat
import errno
import contextlib

import util
import config_parser
import mcc_config

from clusterhost import produce_ABClusterHost 

_logger = logging.getLogger(__name__)

class ShutdownException(Exception):
    """Exception thrown when shutdown command arrives"""
    pass

class ReplyJsonEncoder(json.JSONEncoder):
    """Specialized encoder for which will serialize the folliowing types, 
    in addition to those handled by JSONEncoder:
    TypeTypes - as an html-friendly version their str() output
    TracebackType - as a list of elements (returned by traceback.extrace_tb
    Any class with the __dict__ attribute is serialized as a dict to a json object. """
    
    def default(self, obj):
        """Overrides the default function in JSONEncoder. Specialized for TypeTypes, 
        TracebackTypes and dict-like types. All other types are passed to 
        JSONEncoder.default()."""
        
        if isinstance(obj, types.TypeType):
            return str(obj).replace('<','&lt;').replace('>', '&gt;')
        if isinstance(obj, types.TracebackType):
            return traceback.extract_tb(obj)
        if hasattr(obj, '__dict__'):
            assert isinstance(vars(obj), dict), str(type(obj)) + ' dict attr has type '+str(type(vars(obj)))
            return vars(obj)
        # Ordinary json serialization
        return json.JSONEncoder.default(self, obj)


def handle_req(req):
    """Primary dispatcher function for messages from the client-frontend. Uses 
    introspection to look for a function named handle_<command name> and
    invokes that to process the message.
    req - incoming message (web server request) from the frontend
    """
    
    h = globals()[ 'handle_'+req['head']['cmd']]
    return h(req, req['body'])

def make_rep(req, body=None):
    """Utility which creates a reply object based on the headers in the request
    object."""
    rep = { 'head': { 'seq': req['head']['seq'] +1,
                     'cmd': req['head']['cmd'], 
                     'rSeq': req['head']['seq'] }}
    if body:
        rep['body'] = body
    
    return rep

def get_cred(body):
    """Get the credentials from the message in the form of a (user, pwd) tuple.
    If there is no ssh object present, or keyBased is present and True, a 
    (None, None) tuple is returned."""
    if not body.has_key('ssh') or util.get_val(body['ssh'], 'keyBased', False):
        return (None, None)
    return (body['ssh']['user'], body['ssh']['pwd'])

def handle_hostInfoReq(req, body):
    """Handler function for hostInfoReq commands. Will connect to the specified
    host through a remote.ClusterHost object to retrieve information.
    req - top level message object
    body - shortcut to the body part of the message
    """
    
    (user, pwd) = get_cred(body)
    with produce_ABClusterHost(body['hostName'], user, pwd) as ch:
        return make_rep(req, { 'host': {'name': ch.host },
                               'hostRes': {'ram':ch.ram, 
                                           'cores': ch.cores, 
                                           'uname': ch.hostInfo.uname,
                                           'installdir': ch.installdir, 
                                           'datadir': ch.hostInfo.pm.join(ch.homedir, 'MySQL_Cluster') }})


def start_proc(proc, body):
    """Start individual process as specified in startClusterReq command.
    proc - the process object in the message
    body - the whole message
    """
    f = proc['file']
    (user, pwd) = get_cred(body)
 
    with produce_ABClusterHost(f['hostName'], user, pwd) as ch:
        pc = proc['procCtrl']
        params = proc['params']
        if f.has_key('autoComplete'): 
            if isinstance(f['autoComplete'], list):
                executable = ch.auto_complete(f['path'], f['autoComplete'], f['name'])
            else:
                executable = ch.auto_complete(f['path'], ['bin', 'sbin', 'scripts', '', ch.path_module.join('..','scripts')], f['name'])
        else:
            executable = ch.path_module.join(f['path'], f['name'])
        
        stdinFile = None
        if f.has_key('stdinFile'):
            assert (ch.file_exists(f['stdinFile'])), 'File ' + f['stdinFile'] + " does not exist on host " + ch.host
            stdinFile = f['stdinFile']

        _logger.debug('Attempting to launch '+executable+' on '+ch.host+
                      ' with pc='+str(pc))

        cmdv = util.params_to_cmdv(executable, params)
	if proc.has_key('isCommand'):
            return ch.execute_command(cmdv, stdinFile)
		
        return ch.exec_cmdv(cmdv, pc, stdinFile)
   

def handle_executeCommandReq(req, body):
    """Handler function for execCommandReq messages. Runs the process specified in by the command property."""
    if body['command'].has_key('isCommand'):
        return make_rep(req, start_proc(body['command'], body))

    return make_rep(req, {'out': start_proc(body['command'], body) })	

    
def handle_createFileReq(req, body):
    """Handler function for createFileReq commands. Creates a file on the remote
    host with content from the message.
    req - top level message object
    body - shortcut to the body part of the message
    """
    
    (user, pwd) = get_cred(body)
    f = body['file']
    
    with produce_ABClusterHost(f['hostName'], user, pwd) as ch:
        pathname = f['path']
        if f.has_key('name'):
            pathname = ch.path_module.join(f['path'], f['name'])
            assert not (f.has_key('autoComplete') and f['autoComplete']) 
            assert not (not (f.has_key('overwrite') and f['overwrite']) and ch.file_exists(pathname)), 'File '+pathname+' already exists on host '+ch.host
            ch.mkdir_p(f['path'])
            with ch.open(pathname, 'w+') as rf:
                rf.write(body['contentString'])

            with ch.open(pathname) as rf:
                assert rf.read() == body['contentString']
        else:
            ch.mkdir_p(f['path'])

    _logger.debug('pathname ' + pathname + ' created')

    return  make_rep(req)

def handle_appendFileReq(req, body):
    """Handler function for appendFileReq commands. Opens two files on the remote
    host, copies from source and appends to destination.
    req - top level message object
    body - shortcut to the body part of the message
    """

    (user, pwd) = get_cred(body)
    assert (body.has_key('sourceFile') and body.has_key('destinationFile'))
    sf = body['sourceFile']
    df = body['destinationFile']
    assert (sf.has_key('path') and sf.has_key('name') and sf.has_key('hostName'))
    assert (df.has_key('path') and df.has_key('name') and df.has_key('hostName'))

    with produce_ABClusterHost(sf['hostName'], user, pwd) as ch:
        sp = ch.path_module.join(sf['path'], sf['name'])
        dp = ch.path_module.join(df['path'], df['name'])

        assert (ch.file_exists(dp)), 'File ' + dp + ' does not exist on host ' + ch.host
        content = None
        with ch.open(sp) as sourceFile:
                content = sourceFile.read()

        assert (ch.file_exists(sp)), 'File ' + sp + ' does not exist on host ' + ch.host        
        with ch.open(dp, 'a+') as destinationFile:
            destinationFile.write(content)

    return make_rep(req)


def handle_shutdownServerReq(req, body):
    """x"""
    if body.has_key('deathkey') and body['deathkey'] == deathkey:
        raise ShutdownException("Shutdown request received")
    time.sleep(util.get_val(body, 'sleeptime', 0))
    return make_rep(req, 'incorrect death key')

def handle_getLogTailReq(req, body):
    """Handler function for getLogTailReq commands. Opens a file on the remote
    host and adds content to reply
    req - top level message object
    body - shortcut to the body part of the message
    """

    (user, pwd) = get_cred(body)
    sf = body['logFile']
    assert (sf.has_key('path') and sf.has_key('name') and sf.has_key('hostName'))

    with produce_ABClusterHost(sf['hostName'], user, pwd) as ch:
        sp = ch.path_module.join(sf['path'], sf['name'])
        assert (ch.file_exists(sp)), 'File ' + sp + ' does not exist on host ' + ch.host
        with ch.open(sp) as logFile:
            return make_rep(req, {'tail': logFile.read()})


from util import _parse_until_delim, parse_properties

def parse_reply(ctx):
    """Return False unless ctx['str'] is an mgmd reply. Assign first line to ctx['reply_type], parse property list and return True otherwise."""
    return _parse_until_delim(ctx, 'reply_type', '\n') and parse_properties(ctx,': ')


class mgmd_reply(dict):
  def __init__(self, s=None):
    if (s):
      ctx = {'str': s, 'properties':self}
      parse_reply(ctx)
      self.reply_type = ctx['reply_type']
      
  def __str__(self):
    return self.reply_type+'\n'+'\n'.join(['{0}: {1}'.format(str(k), str(self[k])) for k in self.keys()])+'\n'


def handle_runMgmdCommandReq(req, body):
    """Handler function for runMgmdCommandReq commands. Opens a new connection to mgmd, sends command, parses reply and wraps reply in mcc Rep object."""

    hostname = body['hostname'].encode('ascii', 'ignore')
    port = body['port']
    with util.socket_shutter(socket.create_connection((hostname, port))) as mgmd:
        mgmd.sendall(body['mgmd_command']+'\n\n')
        s = mgmd.recv(4096)
        
    status = mgmd_reply(s)
    sd = {}
    for nk in status.keys():
        if 'node.' in nk:
            (x, n, k) = nk.split('.')
            if not sd.has_key(n):
                sd[n] = {}
            sd[n][k] = status[nk]
                    
    return make_rep(req, { 'reply_type': status.reply_type, 'reply_properties':sd})
                

def handle_getConfigIni(req, body):
    (user, pwd) = get_cred(body)
    cf = body['configFile']
    assert (cf.has_key('path') and cf.has_key('name') and cf.has_key('hostName'))

    with produce_ABClusterHost(sf['hostName'], user, pwd) as ch:
        sp = ch.path_module.join(sf['path'], sf['name'])
        assert (ch.file_exists(sp)), 'File ' + sp + ' does not exist on host ' + ch.host
        with ch.open(sp) as ini:
            return make_rep(req, {'config': config_parser.parse_cluster_config_ini_(ini)})
    
def handle_getNdbConfig(req, body):
    (user, pwd) = get_cred(body)

    with produce_ABClusterHost(body['hostName'], user, pwd) as ch:
        ndb_config = ch.path_module.join(body['installpath'], 'ndb_config')
        return make_rep(req, { 'ndb_config': json.dumps(util.xml_to_python(ch.exec_cmdv([ndb_config, '--configinfo', '--xml']))) })
    
                
def log_thread_name():
    """Utility for dumping thread id in the log."""
    cur_thread = threading.current_thread()
    _logger.debug("cur_thread="+str(cur_thread.name))


class ConfiguratorHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    """Handler class for web server requests to the configurator backend. To be 
    used with a BaseHTTPServer.HTTPServer to create a working server."""
        
    def _send_as_json(self, obj):
        self.send_response(200)
        self.send_header('Content-type', 'text/json')
        self.end_headers()
        self.server.logger.debug('Will send: %s', json.dumps(obj, indent=2, cls=ReplyJsonEncoder))
        json.dump(obj, self.wfile, cls=ReplyJsonEncoder)       
        
    def _do_file_req(self, rt):
        """Handles file requests. Attempts to guess 
        the MIME type and set the Content-Type accordingly."""
        try:
            log_thread_name()
            if self.path == '/':
               self.path = '/'+mcc_config.MCC_BROWSER_START_PAGE
            self.server.logger.debug(rt+' fdir='+self.server.opts['fdir']+ " path="+os.path.normpath(self.path))
            fn = os.path.join(self.server.opts['fdir'], os.path.normpath(self.path[1:]))
            try:
                os.stat(fn)                
            except OSError as ose:
                self.server.logger.exception(rt + ' '+self.path+ ' failed')
                if ose.errno == errno.ENOENT:
                    self.send_error(404, self.path+'=> file://'+ose.filename+' does not exist')
                    return
                raise

            self.send_response(200)
            (ct, enc) = mimetypes.guess_type(self.path)
            if (ct): 
                self.send_header('Content-type',  ct)
            if (enc):
                self.send_header('Content-Encoding',  enc)
            self.end_headers()
            if rt == 'GET':
                with open(fn, "rb") as f:
                    self.wfile.write(f.read()+'\r\n\r\n')
            
        except:
            self.server.logger.exception(rt + ' '+self.path+ ' failed')
            self.send_error(500,'Unexpected exception occured while processing: '+rt+' '+self.path+'\n'+traceback.format_exc()) # Some other number
        
    def do_HEAD(self):
        """Handles HEAD requests by returning the headers without the body if file can be stated."""

        self._do_file_req('HEAD')

    def do_GET(self):
        """Handles GET requests by returning the specified file."""
        self._do_file_req('GET')

    def do_POST(self):
        """Handles POST requests, in the form of json-serialized command (request) 
        objects, from the frontend."""
        log_thread_name()
        try:
            # Assume all post requests are json
            assert  'application/json' in self.headers['Content-Type']
            msg = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            dbgmsg = copy.deepcopy(msg)
            if (dbgmsg['body']['ssh'].has_key('pwd')):
                dbgmsg['body']['ssh']['pwd'] = '*' * len(dbgmsg['body']['ssh']['pwd'])
            self.server.logger.debug('--> ' + dbgmsg['head']['cmd'] + ':')
            self.server.logger.debug(pprint.pformat(dbgmsg))
            rep = make_rep(msg)
            try: 
                rep = handle_req(msg)
            except ShutdownException:
                self.server.shutdown()
            except:
                #traceback.print_exc()
                self.server.logger.exception('POST request failed:')       
                (etype, eobj, etb) = sys.exc_info()
                rep['stat'] = {
                               'errMsg': util.html_rep(eobj),
                               'exType': etype, 
                               'exObj': eobj,
                               'exTrace': etb }

            self.server.logger.debug('<-- ' + rep['head']['cmd'] + ':')
            self.server.logger.debug(pprint.pformat(rep))
            self._send_as_json(rep)
        except:
            traceback.print_exc()
            self.server.logger.critical('Internal server error:')
            self.server.logger.exception('Unexpected exception:')
            self.send_error(500, 'Server Exception\n'+traceback.format_exc()+'while processeing request:\n'+str(self.headers))

    def log_message(self, fmt, *args):
        """Overrides the default implementation which logs to stderr"""
        self.server.logger.info(msg=(fmt%args))


class ConfiguratorServer(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
    """Specialization of HTTPServer which adds ssl wrapping, shutdown on close and MT (by also inheriting SocketServer.ThreadingMixIn."""
    def __init__(self,  opts):
        # Cannot use super() here, as BaseServer is not a new-style class
        SocketServer.TCPServer.__init__(self, (opts['server'], opts['port']), ConfiguratorHandler)
        self.opts = opts
        self.logger = logging.getLogger(str(self.__class__))
    
    def get_request(self):
        """Override get_request from SocketServer.TCPServer so that we can wrap
        the socket in an ssl socket when using ssl."""
        sock,addr = SocketServer.TCPServer.get_request(self)
        if util.get_val(self.opts, 'ssl', False):
            if self.opts['ca_certs']:
                cert_reqs = ssl.CERT_REQUIRED
            else:
                cert_reqs = ssl.CERT_NONE
                
            ssl_sock = ssl.wrap_socket(sock, certfile=self.opts['certfile'], keyfile=self.opts['keyfile'], 
                                       cert_reqs=cert_reqs, ca_certs=self.opts['ca_certs'], server_side=True)
            #self.logger.debug('ssl_sock.getpeercert()='+str(ssl_sock.getpeercert())
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

configdir = None
basedir = None
deathkey = None
        
def main(prefix, cfgdir):
    """Server's main-function which parses the command line and starts up the server accordingly.
    """
    global configdir 
    global basedir 
    configdir = cfgdir
    basedir = prefix
    frontend = os.path.join(cfgdir, 'frontend')
    if not os.path.exists(os.path.join(frontend, 'dojo')):
        dojoz = zipfile.ZipFile(os.path.join(frontend, 'dojo.zip'), 'r')
        dojoz.extractall(path=frontend)
        dojoz.close()

    def_server_name = 'localhost'
    
    if hasattr(webbrowser, 'WindowsDefault') and isinstance(webbrowser.get(), webbrowser.WindowsDefault):
      # If on windows and using IE we do this to avoid IE9 bug (doesn't affect other versions of IE, there is no easy way to test
      def_server_name = socket.gethostname()
 
    cmdln_parser = optparse.OptionParser()
    cmdln_parser.add_option('-N', '--server_name', action='store', type='string', default=def_server_name, 
                            help='server name: [default: %default ]')
    cmdln_parser.add_option('-p', '--port',
                  action='store', type='int', dest='port', default=8081,
                  help='port for the webserver: [default: %default]')
    cmdln_parser.add_option('-n', '--no-browser', action='store_true',
                            help='do not open the server\'s start page in a browser.')
    cmdln_parser.add_option('-s', '--browser-start-page', action='store', type='string',
                            dest='browser_start_page', default=mcc_config.MCC_BROWSER_START_PAGE,
                            help='start page for browser: [default: %default]')
    
    cmdln_parser.add_option('-d', '--debug-level', action='store', type='string', 
                            default='WARNING', 
                            help='Python logging module debug level (DEBUG, INFO, WARNING, ERROR or CRITICAL). [default: %default]')
    
    cmdln_parser.add_option('-o', '--server-log-file', action='store', type='string', 
                            default=os.path.join(tempfile.gettempdir(),'ndb_setup-'+str(os.getpid())+'.log'), 
                            help='log requests to this file. The value - means log to stderr: [default: %default]')
    # option for level/logcfg file
 
    cmdln_parser.add_option('-S', '--use-https', action='store_true', help='use https to secure communication with browser.')

    cmdln_parser.add_option('-c', '--cert-file', action='store', type='string', default=os.path.join(cfgdir, 'cfg.pem'), help='file containing X509 certificate which identifies the server (possibly self-signed): [default: %default]')
    cmdln_parser.add_option('-k', '--key-file', action='store', type='string', help='file containing private key when if not included in cert-file: [default: %default]')
    cmdln_parser.add_option('-a', '--ca-certs-file', action='store', type='string', help='file containing list of client certificates allowed to connect to the server [default: %default (no client authentication)]')

    (options, arguments) = cmdln_parser.parse_args()

    dbglvl = getattr(logging, options.debug_level, logging.DEBUG)
    fmt = '%(asctime)s: %(levelname)s [%(funcName)s;%(filename)s:%(lineno)d]: %(message)s '       
    if options.server_log_file == '-':
        logging.basicConfig(level=dbglvl, format=fmt)
    else:
        logging.basicConfig(level=dbglvl, format=fmt, filename=options.server_log_file)

    srvopts = { 'server' : options.server_name,
               'port': options.port,
               'cdir': cfgdir,
               'fdir': os.path.join(cfgdir, 'frontend') }
    
    if options.use_https:
        srvopts['ssl'] = True
        srvopts['certfile'] = options.cert_file
        srvopts['keyfile'] = options.key_file
        srvopts['ca_certs'] = options.ca_certs_file
        
    print 'Starting web server on port ' + repr(options.port)
    url_host = options.server_name
    if url_host == '':
        url_host = 'localhost'
    if options.use_https:
        url = 'https://{0}:{opt.port}/{opt.browser_start_page}'.format(url_host, opt=options)
    else:
        url = 'http://{0}:{opt.port}/{opt.browser_start_page}'.format(url_host, opt=options)
        
    httpsrv = None
    global deathkey
    deathkey = random.randint(100000, 1000000)
    print 'deathkey='+str(deathkey)
#    dkf = open('deathkey.txt','w')
#    dkf.write(str(deathkey))
#    dkf.close()
#    os.chmod('deathkey.txt', stat.S_IRUSR)
    try:
        httpsrv = ConfiguratorServer(srvopts)
        if not options.no_browser:
            try:
                webbrowser.open_new(url)
            except:
                logging.exception('Failed to control browser: ')
                print 'Could not control your browser. Try to opening '+url+' to launch the application.'
            else:
                print 'The application should now be running in your browser.\n(Alternatively you can navigate to '+url+' to start it)'
        else:
            print 'Navigate to '+url+' to launch the application.'

        httpsrv.serve_forever()
    except KeyboardInterrupt:
        print '^C received, shutting down web server'
    except:
        traceback.print_exc()
    finally:
        if httpsrv:
            httpsrv.socket.close()
        #os.remove('deathkey.txt')

