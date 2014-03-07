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

"""
XML Test Runner for PyUnit
"""

# Written by Sebastian Rittau <srittau@jroger.in-berlin.de> and placed in
# the Public Domain. With contributions by Paolo Borelli and others.
# Modified by Dyre Tjeldvoll

__version__ = "0.0"

import sys
import time
import traceback
import socket
import platform

import unittest
if not hasattr(unittest, 'TextTestResult'):
    import unittest2 as unittest

import xml.etree.ElementTree as ET

class XMLTestResult(unittest.TextTestResult):
    def __init__(self, stream, descriptions, verbosity):
        super(type(self), self).__init__(stream, descriptions, verbosity)
        self._tb = ET.TreeBuilder()
        self._ts = {}
        self._tc = {}
        
    def startTest(self, test):
        "Called when the given test is about to be run"
        super(type(self), self).startTest(test)
        (c,m) = test.id().rsplit('.',1)
        self._tc = { 'element': self._tb.start('testcase', 
                                               {'classname':c, 'name':m }),
                     'stime' : time.time() }
        

    def startTestRun(self):
        """Called once before any tests are executed.

        See startTest for a method called before each test.
        """
        super(type(self), self).startTestRun()
        self._ts = { 'element': 
                     self._tb.start('testsuite', 
                                    {'hostname': socket.gethostname(),
                                   'id':'0' }),
                     'stime': time.time() }

    def _addOut(self, kind):
        buf = getattr(self, '_std{0}_buffer'.format(kind))
        if buf:
            tag = 'system-{0}'.format(kind)
            self._tb.start(tag, {})
            self._tb.data(buf.getvalue())
            self._tb.end(tag)

    def stopTest(self, test):
        """Called when the given test has been run"""
        # Need to do this before calling the inherited method, since
        # that will clear the buffers
        try:
            try:
                self._addOut('out')
                self._addOut('err')
            finally:
                super(type(self), self).stopTest(test)
                #print 'super().stopTest called'

            self._tc['element'].set('time', str(time.time() - self._tc['stime']))
            self._tb.end('testcase')
            self._tc['element'] = None
            self._tc['stime'] = None
        except:
            traceback.print_exc()
            raise
         
    def stopTestRun(self):
        """Called once after all tests are executed.

        See stopTest for a method called after each test.
        """
        #print 'Calling super().stopTestRun'
        super(type(self), self).stopTestRun()
        #print 'After super().stopTestRun'
        self._ts['element'].set('errors',str(len(self.errors)))
        # TODO - do we need to add unexpected successes here?
        self._ts['element'].set('failures',str(len(self.failures)))
        self._ts['element'].set('skipped',str(len(self.skipped)))
        self._ts['element'].set('tests', str(self.testsRun))
        self._ts['element'].set('time',str(time.time()-self._ts['stime']))
        self._ts['element'].set('timestamp',str(time.localtime(time.time())))
        
        self._tb.end('testsuite')

    def _addEx(self, kind, excinf):
        try:
            self._tb.start(kind, {'message':str(excinf[1]), 
                              'type':excinf[0].__name__})
            self._tb.data(''.join(traceback.format_tb(excinf[2])))
            self._tb.end(kind)
            self._tc['element'].set('status', kind)
        except:
            traceback.print_exc()
            raise

    def addError(self, test, err):
        """Called when an error has occurred. 'err' is a tuple of values as
        returned by sys.exc_info().
        """
        super(type(self), self).addError(test, err)
        self._addEx('error', err)

    def addFailure(self, test, err):
        """Called when an error has occurred. 'err' is a tuple of values as
        returned by sys.exc_info()."""
        super(type(self), self).addFailure(test, err)
        self._addEx('failure', err)

    def addSuccess(self, test):
        "Called when a test has completed successfully"
        super(type(self), self).addSuccess(test)
        self._tc['element'].set('status', 'success')

    def addSkip(self, test, reason):
        """Called when a test is skipped."""
        super(type(self), self).addSkip(test, reason)
        self._tb.start('skipped', {})
        self._tb.data(reason)
        self._tb.end('skipped')
        self._tc['element'].set('status', 'skipped because: {0}'.format(reason))

    def addExpectedFailure(self, test, err):
        """Called when an expected failure/error occured."""
        super(type(self), self).addExpectedFailure(test, err)
        # TODO - can we utilize this?

    def addUnexpectedSuccess(self, test):
        """Called when a test was expected to fail, but succeed."""
        super(type(self), self).addUnexpectedSuccess(test, err)
        # TODO - can we utilize this?


    ### 
    def set_suite_name(self, suite):
        self._ts['element'].set('name', suite.__class__.__name__)
        if suite.__module__ != '__Main__':
            self._ts['element'].set('package', suite.__module__)
        
    def xml_str(self):
        return ET.tostring(self._tb.close())

class XMLTestRunner(unittest.TextTestRunner):
    def __init__(self, stream=sys.stderr, descriptions=True, verbosity=True, 
    failfast=False, buffer=True, resultclass=XMLTestResult):
        super(type(self), self).__init__(stream, descriptions, verbosity, 
        failfast, True, resultclass)

    def run(self, test):
        XMLTestResult__ = None
        junitrep = None
        try:
            XMLTestResult__ = super(type(self), self).run(test)
            XMLTestResult__.set_suite_name(test)
            xmlstr = XMLTestResult__.xml_str()

            junitrep = open('junit_report_{0}.{1}.xml'.format(test.__module__, test.__class__.__name__),'w')
            junitrep.write(xmlstr)
        except:
            traceback.print_exc()
            XMLTestResult__.xml_str()
        finally:
            if junitrep:
                junitrep.close()

        return XMLTestResult__
