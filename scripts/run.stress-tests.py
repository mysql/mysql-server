#!/usr/bin/python
"""
A script for running our stress tests repeatedly to see if any fail.

Runs a list of stress tests in parallel, reporting passes and collecting
failure scenarios until killed.  Runs with different table sizes,
cachetable sizes, and numbers of threads.

Suitable for running on a dev branch, or a release branch, or main.

Just run the script from within a branch you want to test.

By default, we stop everything, update from svn, rebuild, and restart the
tests once a day.
"""

from glob import glob
from optparse import OptionParser
import os
from Queue import Queue
from random import randrange
from resource import setrlimit, RLIMIT_CORE
from shutil import copy, copytree, move, rmtree
from signal import signal, SIGHUP, SIGINT, SIGPIPE, SIGALRM, SIGTERM
from subprocess import call, Popen, PIPE, STDOUT
import sys
from tempfile import mkdtemp, mkstemp
import time
import thread
from threading import Event, Thread, Timer
import threading
import traceback

__version__   = '$Id$'
__copyright__ = """Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved.

                The technology is licensed by the Massachusetts Institute
                of Technology, Rutgers State University of New Jersey, and
                the Research Foundation of State University of New York at
                Stony Brook under United States of America Serial
                No. 11/760379 and to the patents and/or patent
                applications resulting from it."""

testnames = ['test_stress1.tdb',
             'test_stress5.tdb',
             'test_stress6.tdb']
recover_testnames = ['recover-test_stress1.tdb',
                     'recover-test_stress2.tdb',
                     'recover-test_stress3.tdb']

verbose = False
debug = False
def dbg(a):
    if debug:
        print >>sys.stderr, a

def setlimits():
    setrlimit(RLIMIT_CORE, (-1, -1))
    os.nice(7)

class TestFailure(Exception):
    pass

class Killed(Exception):
    pass

class TestRunnerBase(object):
    def __init__(self, scheduler, tokudb, rev, execf, tsize, csize, test_time, savedir, log):
        self.scheduler = scheduler
        self.tokudb = tokudb
        self.rev = rev
        self.execf = execf
        self.tsize = tsize
        self.csize = csize
        self.test_time = test_time
        self.savedir = savedir
        self.logfile = os.path.join(log, ('%s-%d-%d' %
                                          (self.execf, self.tsize, self.csize)))
        self.nruns = 0
        self.env = os.environ
        self.env['LD_LIBRARY_PATH'] = (os.path.join(self.tokudb, 'lib') +
                                       ':' + self.env['LD_LIBRARY_PATH'])
        self.rundir = None
        self.tmplog = None
        self.tmplogname = None
        self.phase = 0
        self.times = [0, 0, 0]

    def __str__(self):
        return '%s(tsize=%d, csize=%d)' % (self.execf, self.tsize, self.csize)

    def run(self):
        srctests = os.path.join(self.tokudb, 'src', 'tests')
        self.rundir = mkdtemp(dir=srctests)
        (tmplog, self.tmplogname) = mkstemp()
        self.tmplog = os.fdopen(tmplog)

        if self.nruns % 2 < 1:
            self.ptquery = 1
        else:
            self.ptquery = randrange(16)
        if self.nruns % 4 < 2:
            self.update = 1
        else:
            self.update = randrange(16)

        self.envdir = ('../%s-%d-%d-%d-%d-%d.dir' %
                       (self.execf, self.tsize, self.csize,
                        self.ptquery, self.update, thread.get_ident()))
        os.mkdir(self.envdir)

        try:
            try:
                self.times[0] = time.time()
                self.setup_test()
                self.times[1] = time.time()
                self.run_test()
                self.times[2] = time.time()
            except Killed:
                pass
            except TestFailure:
                savedir = self.save()
                self.print_failure(savedir)
            else:
                self.print_success()
        finally:
            fullenvdir = os.path.join(self.rundir, self.envdir)
            rmtree(fullenvdir, ignore_errors=True)
            self.envdir = None
            rmtree(self.rundir, ignore_errors=True)
            self.rundir = None
            self.tmplog.close()
            self.tmplog = None
            if os.path.exists(self.tmplogname):
                os.remove(self.tmplogname)
            self.tmplogname = None
            self.times = [0, 0, 0]
            self.nruns += 1

    def save(self):
        savepfx = ("%s-%s-%d-%d-%d-%d-%s-" % (self.execf,
                                             self.rev,
                                             self.tsize,
                                             self.csize,
                                             self.ptquery,
                                             self.update,
                                             self.phase))
        savedir = mkdtemp(dir=self.savedir, prefix=savepfx)
        def targetfor(path):
            return os.path.join(savedir, os.path.basename(path))
        if os.path.exists(self.tmplogname):
            move(self.tmplogname, targetfor("output.txt"))
        for core in glob(os.path.join(self.rundir, "core*")):
            move(core, targetfor(core))
        if os.path.exists(self.envdir):
            copytree(self.envdir, targetfor(self.envdir))
        fullexecf = os.path.join(self.tokudb, 'src', 'tests', self.execf)
        copy(fullexecf, targetfor(fullexecf))
        for lib in glob(os.path.join(self.tokudb, 'lib', '*')):
            copy(lib, targetfor(lib))
        return savedir

    def print_success(self):
        f = open(self.logfile, 'w+')
        print >>f, ('"%s",%s,%d,%d,%d,%d,%s,%s,%s,PASS' %
                    (self.execf,
                     self.rev,
                     self.tsize,
                     self.csize,
                     self.ptquery,
                     self.update,
                     time.ctime(self.times[0]),
                     time.ctime(self.times[1]),
                     time.ctime(self.times[2])))
        f.close()
        self.scheduler.passed += 1
        if verbose:
            print >>sys.stderr, ('[PASS=%d FAIL=%d] '
                                 'PASSED %s tsize=%d csize=%d '
                                 'ptquery=%d update=%d' %
                                 (self.scheduler.passed, self.scheduler.failed,
                                  self.execf, self.tsize, self.csize,
                                  self.ptquery, self.update))

    def print_failure(self, savedir):
        f = open(self.logfile, 'w+')
        print >>f, ('"%s",%s,%d,%d,%d,%d,%s,%s,%s,PASS' %
                    (self.execf,
                     self.rev,
                     self.tsize,
                     self.csize,
                     self.ptquery,
                     self.update,
                     time.ctime(self.times[0]),
                     time.ctime(self.times[1]),
                     time.ctime(self.times[2])))
        f.close()
        self.scheduler.failed += 1
        print >>sys.stderr, ('[PASS=%d FAIL=%d] '
                             'FAILED %s tsize=%d csize=%d '
                             'ptquery=%d update=%d' %
                             (self.scheduler.passed, self.scheduler.failed,
                              self.execf, self.tsize, self.csize,
                              self.ptquery, self.update))
        print >>sys.stderr, 'Saved environment to %s' % savedir

    def waitfor(self, proc):
        while proc.poll() is None:
            self.scheduler.stopping.wait(1)
            if self.scheduler.stopping.isSet():
                os.kill(proc.pid, SIGTERM)
                raise Killed()

    def defaultargs(self, timed):
        a = ['-v',
             '--envdir', self.envdir,
             '--num_elements', str(self.tsize),
             '--cachetable_size', str(self.csize)]
        if timed:
            a += ['--num_seconds', str(self.test_time),
                  '--no-crash_on_update_failure',
                  '--num_ptquery_threads', str(self.ptquery),
                  '--num_update_threads', str(self.update)]
        return a

    def spawn_child(self, mode, timed):
        proc = Popen([self.execf, mode] + self.defaultargs(timed),
                     executable=os.path.join('..', self.execf),
                     env=self.env,
                     cwd=self.rundir,
                     preexec_fn=setlimits,
                     stdout=self.tmplog,
                     stderr=STDOUT)
        self.waitfor(proc)
        return proc.returncode

class TestRunner(TestRunnerBase):
    def setup_test(self):
        self.phase = "create"
        if self.spawn_child('--only_create', False) != 0:
            raise TestFailure('%s crashed during --only_create.' % self.execf)

    def run_test(self):
        self.phase = "stress"
        if self.spawn_child('--only_stress', True) != 0:
            raise TestFailure('%s crashed during --only_stress.' % self.execf)

class RecoverTestRunner(TestRunnerBase):
    def setup_test(self):
        self.phase = "test"
        if self.spawn_child('--test', True) == 0:
            raise TestFailure('%s did not crash during --test' % self.execf)

    def run_test(self):
        self.phase = "recover"
        if self.spawn_child('--recover', False) != 0:
            raise TestFailure('%s crashed during --recover' % self.execf)

class Worker(Thread):
    def __init__(self, scheduler):
        super(Worker, self).__init__()
        self.scheduler = scheduler

    def run(self):
        dbg('%s starting.' % self)
        while not self.scheduler.stopping.isSet():
            test_runner = self.scheduler.get()
            try:
                test_runner.run()
            except Exception, e:
                print >>sys.stderr, 'Fatal error in worker thread.'
                traceback.print_exc()
                print >>sys.stderr, 'Killing all workers.'
                self.scheduler.error = e
                self.scheduler.stop()
            if not self.scheduler.stopping.isSet():
                self.scheduler.put(test_runner)
        dbg('%s exiting.' % self)

class Scheduler(Queue):
    def __init__(self, nworkers):
        Queue.__init__(self)
        if verbose:
            print >>sys.stderr, 'Initializing scheduler with %d jobs.' % nworkers
        self.nworkers = nworkers
        self.passed = 0
        self.failed = 0
        self.workers = []
        self.stopping = Event()
        self.timer = None
        self.error = None

    def run(self, timeout):
        if verbose:
            print >>sys.stderr, 'Starting workers.'
        self.stopping.clear()
        for i in range(self.nworkers):
            w = Worker(self)
            self.workers.append(w)
            w.start()
        if timeout != 0:
            self.timer = Timer(timeout, self.stop)
            self.timer.start()
        while not self.stopping.isSet():
            try:
                for w in self.workers:
                    if self.stopping.isSet():
                        break
                    w.join(timeout=1.0)
            except (KeyboardInterrupt, SystemExit):
                self.stop()
                self.join()
                sys.exit(0)
        else:
            self.join()

    def join(self):
        if self.timer is not None:
            self.timer.cancel()
        while len(self.workers) > 0:
            self.workers.pop().join()

    def stop(self):
        if verbose:
            print >>sys.stderr, 'Stopping workers.'
        self.stopping.set()

def rebuild(tokudb, cc):
    env = os.environ
    env['CC'] = cc
    env['DEBUG'] = '0'
    if verbose:
        print >>sys.stderr, 'Updating from svn.'
    call(['svn', 'up'], cwd=tokudb)
    if verbose:
        print >>sys.stderr, 'Building tokudb.'
    r = call(['make', '-s', 'clean'],
             cwd=tokudb, env=env)
    if r != 0:
        print >>sys.stderr, 'Build failed.  Do you need to source the icc env script?'
        sys.exit(r)
    r = call(['make', 'fastbuild'],
             cwd=tokudb, env=env)
    if r != 0:
        print >>sys.stderr, 'Build failed.  Do you need to source the icc env script?'
        sys.exit(r)
    r = call(['make'] + testnames + recover_testnames,
             cwd=os.path.join(tokudb, 'src', 'tests'), env=env)
    if r != 0:
        print >>sys.stderr, 'Build failed.  Do you need to source the icc env script?'
        sys.exit(r)

def revfor(tokudb):
    proc = Popen("svn info | awk '/Revision/ {print $2}'",
                 shell=True, cwd=tokudb, stdout=PIPE)
    (out, err) = proc.communicate()
    rev = out.strip()
    if verbose:
        print >>sys.stderr, 'Using tokudb at r%s.' % rev
    return rev

def main(opts):
    if opts.build:
        rebuild(opts.tokudb, opts.cc)
    rev = revfor(opts.tokudb)
    if not os.path.exists(opts.log):
        os.mkdir(opts.log)
    if not os.path.exists(opts.savedir):
        os.mkdir(opts.savedir)
    if verbose:
        print >>sys.stderr, 'Saving pass/fail logs to %s.' % opts.log
        print >>sys.stderr, 'Saving failure environments to %s.' % opts.savedir

    scheduler = Scheduler(opts.jobs)

    runners = []
    for tsize in [2000, 200000, 50000000]:
        for csize in [50 * tsize, 1000 ** 3]:
            for test in testnames:
                runners.append(TestRunner(scheduler, opts.tokudb, rev,
                                          test, tsize, csize, opts.test_time,
                                          opts.savedir, opts.log))
            for test in recover_testnames:
                runners.append(RecoverTestRunner(scheduler, opts.tokudb, rev,
                                                 test, tsize, csize, opts.test_time,
                                                 opts.savedir, opts.log))

    for runner in runners:
        scheduler.put(runner)

    try:
        while scheduler.error is None:
            scheduler.run(opts.rebuild_period)
            if scheduler.error is not None:
                raise scheduler.error
            rebuild(opts.tokudb, opts.cc)
            rev = revfor(opts.tokudb)
            for runner in runners:
                runner.rev = rev
    except (KeyboardInterrupt, SystemExit):
        sys.exit(0)

# relpath implementation for python <2.6
# from http://unittest-ext.googlecode.com/hg-history/1df911640f7be239e58fb185b06ac2a8489dcdc4/unittest2/unittest2/compatibility.py
if not hasattr(os.path, 'relpath'):
    if os.path is sys.modules.get('ntpath'):
        def relpath(path, start=os.path.curdir):
            """Return a relative version of a path"""

            if not path:
                raise ValueError("no path specified")
            start_list = os.path.abspath(start).split(os.path.sep)
            path_list = os.path.abspath(path).split(os.path.sep)
            if start_list[0].lower() != path_list[0].lower():
                unc_path, rest = os.path.splitunc(path)
                unc_start, rest = os.path.splitunc(start)
                if bool(unc_path) ^ bool(unc_start):
                    raise ValueError("Cannot mix UNC and non-UNC paths (%s and %s)"
                                                                        % (path, start))
                else:
                    raise ValueError("path is on drive %s, start on drive %s"
                                                        % (path_list[0], start_list[0]))
            # Work out how much of the filepath is shared by start and path.
            for i in range(min(len(start_list), len(path_list))):
                if start_list[i].lower() != path_list[i].lower():
                    break
            else:
                i += 1

            rel_list = [os.path.pardir] * (len(start_list)-i) + path_list[i:]
            if not rel_list:
                return os.path.curdir
            return os.path.join(*rel_list)

    else:
        # default to posixpath definition
        def relpath(path, start=os.path.curdir):
            """Return a relative version of a path"""

            if not path:
                raise ValueError("no path specified")

            start_list = os.path.abspath(start).split(os.path.sep)
            path_list = os.path.abspath(path).split(os.path.sep)

            # Work out how much of the filepath is shared by start and path.
            i = len(os.path.commonprefix([start_list, path_list]))

            rel_list = [os.path.pardir] * (len(start_list)-i) + path_list[i:]
            if not rel_list:
                return os.path.curdir
            return os.path.join(*rel_list)

    os.path.relpath = relpath

if __name__ == '__main__':
    a0 = os.path.abspath(sys.argv[0])
    usage = '%prog [options]\n' + __doc__
    parser = OptionParser(usage=usage)
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose', default=False)
    parser.add_option('-d', '--debug', action='store_true', dest='debug', default=False)
    default_toplevel = os.path.dirname(os.path.dirname(a0))
    parser.add_option('-l', '--log', type='string', dest='log',
                      default='/tmp/run.stress-tests.log',
                      help='where to save logfiles')
    parser.add_option('-s', '--savedir', type='string', dest='savedir',
                      default='/tmp/run.stress-tests.failures',
                      help='where to save environments and extra data for failed tests')
    parser.add_option('--tokudb', type='string', dest='tokudb',
                      default=default_toplevel,
                      help=('top of the tokudb tree (contains newbrt/ and src/) [default=%s]' % os.path.relpath(default_toplevel)))
    parser.add_option('-t', '--test_time', type='int', dest='test_time',
                      default=600,
                      help='time to run each test, in seconds [default=600]'),
    parser.add_option('-j', '--jobs', type='int', dest='jobs',
                      default=8,
                      help='how many concurrent tests to run [default=8]')
    parser.add_option('--no-build', action='store_false', dest='build', default=True,
                      help="don't build before testing [default=do build]")
    parser.add_option('--rebuild_period', type='int', dest='rebuild_period',
                      default=60 * 60 * 24,
                      help='how many seconds between svn up and rebuild, 0 means never rebuild [default=24 hours]')
    parser.add_option('--cc', type='string', dest='cc', default='icc',
                      help='which compiler to use [default=icc]')
    (opts, args) = parser.parse_args()
    if len(args) > 0:
        parser.print_usage()
        sys.exit(1)
    debug = opts.debug
    verbose = opts.verbose or opts.debug
    main(opts)
