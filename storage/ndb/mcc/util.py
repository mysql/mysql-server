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

"""
Common utilities needed by the configurator backend modules.
"""
import socket
import xml.sax
import errno
import stat
import platform
import logging
import contextlib

_logger = logging.getLogger(__name__)

@contextlib.contextmanager
def socket_shutter(sock):
    """x"""
    try:
        yield sock
    finally:
        sock.shutdown(socket.SHUT_RDWR)
        sock.close()


def is_set(val, mask):
    """Return True if bits in mask are also set in val."""
    
    return val & mask == mask

def is_dir(attrs):
    """Returns true of attrs is the stat attrs for a directory, False otherwise."""
    
    return stat.S_ISDIR(attrs.st_mode)


def mock_msg(cmd, body):
    """Wrap body in a dummy message header for cmd."""
    
    return { 'head': { 'seq': 0, 'cmd' : cmd}, 'body': body, 'reply': None}

def try_connect(url_host, port, success=True, retdict={}):
    """Test connectability of url_host and port. Return success if connection attempt 
    was successful. If the connection attempt throws a socket.error and its errno is
    a key in retdict, the corresponding retdict value is returned. Any other exception 
    propagated."""
    
    s = None
    try:
        s = socket.create_connection((url_host, port))
    except socket.error as err:
        if retdict.has_key(err.errno):
            return retdict[err.errno]
        _logger.debug('err.errno='+str(err.errno)+' not found in retdict='+str(retdict))
        raise
    else:
        return success
    finally:
        if s:
            s.shutdown(socket.SHUT_RDWR)
            s.close()

def _retdict(name, val):
    rd = { getattr(errno,name) : val }
    wsa = 'WSA'+name
    if hasattr(errno, wsa):
        rd[getattr(errno, wsa)] = val
    return rd 
            
def is_port_available(url_host,port):
    """Predicate for determining if a port is available. Returns False if connection to
    url_host,port succeeds, and True if it throws connection refused."""
    
    return try_connect(url_host, port, False, _retdict('ECONNREFUSED', True))
 
class NoPortAvailableException(Exception):
    """Exception thrown if no unused port can be found."""
    
    def __init__(self, url_host, bp, ep):
        self.host = url_host
        self.bp = bp
        self.ep = ep
        
    def __str__(self):
        return 'No available ports in range [{_.bp},{_.ep}] on url_host {_.url_host}'.format(_=self)
    
def first_available_port(url_host, bport,eport):
    """Locate the first unused port on url_host in the interval [bport,eport]. Returns 
    the port number if successful. Throws NoPortAvailableException otherwise."""
    
    for p in range(bport, eport+1):
        if is_port_available(url_host, p):
            return p;
    raise NoPortAvailableException(url_host, bport, eport)

def html_rep(obj):
    """Format an object in an html-friendly way. That is; convert any < and > characters
    found in the str() representation to the corresponding html-escape sequence."""
    
    s = str(obj)
    if s == '':
        s = repr(obj)            
    return s.strip().replace('<','&lt;').replace('>', '&gt;')

def get_val(d, k, default=None):
    """
    @type d:  dict
    @param d: Value store
    @type k: str
    @param k: Key to get value for
    @param default: Return this value if key L{k} cannot be found
    @rtype: str
    @return: Value corresponding to key L{k} or L{default} 
    """
    if not d.has_key(k):
        return default
    return d[k]

def get_fmask(perm, role):
    """Get the stat permission mask value corresponding to perm (X,R,W) 
    and role (USR, GRP, OTH)."""
    return getattr(stat, 'S_I'+perm+role)

class Param(dict):
    """Specialization of dict representing a command line parameter.
    Extending dict allows easy serialization as Json."""
    
    def __init__(self, name, val=None, sep='='):
        if type(name) == dict or type(name) == Param:
            for k in name.keys():
                self[k] = name[k]
            return
        self['name'] = name
        if val:
            self['val'] = val
            self['sep'] = sep          
        assert self['name'] == name
 
    def __str__(self):
        if self.has_key('val'):
            return self['name']+get_val(self, 'sep', '=') +str(self['val'])
        return self['name']


def params_to_cmdv(executable, params):
    return [ executable ] + [str(Param(p)) for p in params['param']]

class ConvertToPython(xml.sax.handler.ContentHandler):
    """Specialized handler to convert xml into a graph of builtin, json-serializable Python 
    objects (dicts and lists)."""
    
    def __init__(self):
        self._estack = []
        
    def startElement(self, name, attrs):
        """Creates a new attribute dict and pushes it on the element stack."""
        
        #print 'start( ', name, ')'
        a = {}
        for an in attrs.getNames():
            a[an] = attrs.getValue(an)
            try:
                a[an] = float(a[an])
                a[an] = int(a[an])
            except:
                pass               
        self._estack.append([name,a])
        
    def endElement(self, name):
        """Pops the element stack and inserts the popped element as a 
        child of the new top"""
        #print 'end(', name, ')'
        if len(self._estack)  > 1:
            e = self._estack.pop()
            self._estack[-1].append(e)
    
    def get_obj(self):
        """Returns the resulting object. Convenience method for accessing the only
        remaining element on the element stack."""
        
        assert len(self._estack) == 1
        return self._estack[0]
    
def xml_to_python(xml_str):
    """Convenience function for parsing an xml-string with sax.parseString 
    and ConvertToPython."""
    
    assert xml_str != None
    cp = ConvertToPython()
    xml.sax.parseString(xml_str, cp, xml.sax.handler.ErrorHandler())
    return cp.get_obj()


def _parse_until_delim(ctx, fld, delim):
    """ Return False unless delim exists in ctx['str']. Assign ctx['str'] excluding delim to ctx[fld] and assign ctx[str] to the remainder, excluding delim, and return True otherwise."""
    i = ctx['str'].find(delim)
    if (i == -1):
        return False
 
    ctx[fld] = ctx['str'][0:i]
    ctx['str'] = ctx['str'][i+len(delim):]
    return True	

def parse_empty_line(ctx):
    """Return False unless ctx[str] starts with an empty line. Consume the empty line and return True otherwise."""
    if ctx['str'].startswith('\n'):
        ctx['str'] = ctx['str'][1:]
        return True
    return False	
  
def parse_property(ctx, delim):
    """Return False unless key and value parsing succeeds. Add kv-pair to ctx['properties'] and return True otherwise."""
    if _parse_until_delim(ctx, 'key', delim) and _parse_until_delim(ctx, 'val', '\n'):
        ctx['properties'][ctx['key']] = ctx['val']
        return True
    return False
  
def parse_properties(ctx, delim='='):
    """Return False unless ctx['str'] is a list of properties, a single property or an empty line. 
    Parse the list of properties and return True otherwise."""
    return parse_property(ctx,delim) and parse_properties(ctx,delim) or parse_empty_line(ctx)
