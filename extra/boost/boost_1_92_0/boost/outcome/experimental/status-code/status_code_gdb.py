import gdb.printing
import gdb
import os

def synthesise_gdb_value_from_string(s):
    '''For when you want to return a synthetic string from children()'''
    return gdb.Value(s + '\0').cast(gdb.lookup_type('char').pointer())

class StatusCodePrinter(object):
    '''Print a system_error2::status_code<T>'''

    def __init__(self, val):
        self.val = val

    def children(self):
        s = str(self.val['_domain'])
        if 'posix_code_domain' in s or 'generic_code_domain' in s:
            yield ('msg', synthesise_gdb_value_from_string(str(self.val['_value']) + ' (' + os.strerror(int(self.val['_value'])) + ')'))
        yield ('domain', self.val['_domain'])
        yield ('value', self.val['_value'])

    def display_hint(self):
        return None

    def to_string(self):
        s = str(self.val['_domain'])
        if 'posix_code_domain' in s or 'generic_code_domain' in s:
            return str(self.val['_value']) + ' (' + os.strerror(int(self.val['_value'])) + ')'
        else:
            return self.val['_value']

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter('system_error2')
    pp.add_printer('system_error2::status_code', '^(boost::)?system_error2::status_code<.*>$', StatusCodePrinter)
    return pp

def register_printers(obj = None):
    gdb.printing.register_pretty_printer(obj, build_pretty_printer(), replace = True)

register_printers(gdb.current_objfile())
