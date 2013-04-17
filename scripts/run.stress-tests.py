#!/usr/bin/env python
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

import logging
import os
import sys
import time

from glob import glob
from logging import debug, info, warning, error, exception
from optparse import OptionParser
from Queue import Queue
from random import randrange, shuffle
from resource import setrlimit, RLIMIT_CORE
from shutil import copy, copytree, move, rmtree
from signal import signal, SIGHUP, SIGINT, SIGPIPE, SIGALRM, SIGTERM
from subprocess import call, Popen, PIPE, STDOUT
from tempfile import mkdtemp, mkstemp
from threading import Event, Thread, Timer

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

def setlimits():
    setrlimit(RLIMIT_CORE, (-1, -1))
    os.nice(7)

class TestFailure(Exception):
    pass

class Killed(Exception):
    pass

class TestRunnerBase(object):
    def __init__(self, scheduler, tokudb, rev, jemalloc, execf, tsize, csize, test_time, savedir):
        self.scheduler = scheduler
        self.tokudb = tokudb
        self.rev = rev
        self.execf = execf
        self.tsize = tsize
        self.csize = csize
        self.test_time = test_time
        self.savedir = savedir

        self.env = os.environ
        libpath = os.path.join(self.tokudb, 'lib')
        if 'LD_LIBRARY_PATH' in self.env:
            self.env['LD_LIBRARY_PATH'] = '%s:%s' % (libpath, self.env['LD_LIBRARY_PATH'])
        else:
            self.env['LD_LIBRARY_PATH'] = libpath

        if jemalloc is not None and len(jemalloc) > 0:
            preload = os.path.normpath(jemalloc)
            if 'LD_PRELOAD' in self.env:
                self.env['LD_PRELOAD'] = '%s:%s' % (preload, self.env['LD_PRELOAD'])
            else:
                self.env['LD_PRELOAD'] = preload

        self.nruns = 0
        self.rundir = None
        self.outf = None
        self.times = [0, 0]
        self.is_large = (tsize >= 10000000)

    def __str__(self):
        return 'TestRunner<%s, %d, %d>' % (self.execf, self.tsize, self.csize)

    def __getitem__(self, k):
        return self.__getattribute__(k)

    def infostr(self):
        return '\t'.join(['%(execf)s',
                          '%(rev)s',
                          '%(tsize)d',
                          '%(csize)d',
                          '%(num_ptquery)d',
                          '%(num_update)d',
                          '%(time)d']) % self

    @property
    def time(self):
        if self.times[0] != 0 and self.times[1] != 0:
            return self.times[1] - self.times[0]
        else:
            return 0

    @property
    def num_ptquery(self):
        if self.nruns % 2 < 1:
            return 1
        else:
            return randrange(16)

    @property
    def num_update(self):
        if self.nruns % 4 < 2:
            return 1
        else:
            return randrange(16)

    @property
    def envdir(self):
        return os.path.join(self.rundir, 'envdir')

    @property
    def prepareloc(self):
        preparename = 'dir.%(execf)s-%(tsize)d-%(csize)d' % self
        return os.path.join(self.tokudb, 'src', 'tests', preparename)

    def prepare(self):
        if os.path.isdir(self.prepareloc):
            debug('%s found existing environment.', self)
            copytree(self.prepareloc, self.envdir)
        else:
            debug('%s preparing an environment.', self)
            self.run_prepare()
            debug('%s copying environment to %s.', self, self.prepareloc)
            copytree(self.envdir, self.prepareloc)

    def run(self):
        srctests = os.path.join(self.tokudb, 'src', 'tests')
        self.rundir = mkdtemp(dir=srctests)

        try:
            outname = os.path.join(self.rundir, 'output.txt')
            self.outf = open(outname, 'w')

            try:
                self.prepare()
                debug('%s testing.', self)
                self.times[0] = time.time()
                self.run_test()
                self.times[1] = time.time()
                debug('%s done.', self)
            except Killed:
                pass
            except TestFailure:
                savedir = self.save()
                self.scheduler.report_failure(self)
                warning('Saved environment to %s', savedir)
            else:
                self.scheduler.report_success(self)
        finally:
            self.outf.close()
            rmtree(self.rundir)
            self.rundir = None
            self.times = [0, 0]
            self.nruns += 1

    def save(self):
        savepfx = '%(execf)s-%(rev)s-%(tsize)d-%(csize)d-%(num_ptquery)d-%(num_update)d-%(phase)s' % self
        savedir = mkdtemp(dir=self.savedir, prefix=savepfx)
        def targetfor(path):
            return os.path.join(savedir, os.path.basename(path))

        copytree(self.rundir, targetfor(self.rundir))
        fullexecf = os.path.join(self.tokudb, 'src', 'tests', self.execf)
        copy(fullexecf, targetfor(fullexecf))
        for lib in glob(os.path.join(self.tokudb, 'lib', '*.so')):
            copy(lib, targetfor(lib))

        return savedir

    def waitfor(self, proc):
        while proc.poll() is None:
            self.scheduler.stopping.wait(1)
            if self.scheduler.stopping.isSet():
                os.kill(proc.pid, SIGTERM)
                raise Killed()

    def spawn_child(self, args):
        proc = Popen([self.execf] + args,
                     executable=os.path.join('..', self.execf),
                     env=self.env,
                     cwd=self.rundir,
                     preexec_fn=setlimits,
                     stdout=self.outf,
                     stderr=STDOUT)
        self.waitfor(proc)
        return proc.returncode

    def prepareargs(self):
        return ['-v',
                '--envdir', 'envdir',
                '--num_elements', str(self.tsize),
                '--cachetable_size', str(self.csize)]

    def testargs(self):
        return ['--num_seconds', str(self.test_time),
                '--no-crash_on_update_failure',
                '--num_ptquery_threads', str(self.num_ptquery),
                '--num_update_threads', str(self.num_update)] + self.prepareargs()

class TestRunner(TestRunnerBase):
    def run_prepare(self):
        self.phase = "create"
        if self.spawn_child(['--only_create'] + self.prepareargs()) != 0:
            raise TestFailure('%s crashed during --only_create.' % self.execf)

    def run_test(self):
        self.phase = "stress"
        if self.spawn_child(['--only_stress'] + self.testargs()) != 0:
            raise TestFailure('%s crashed during --only_stress.' % self.execf)

class RecoverTestRunner(TestRunnerBase):
    def run_prepare(self):
        self.phase = "create"
        if self.spawn_child(['--only_create', '--test'] + self.prepareargs()) != 0:
            raise TestFailure('%s crashed during --only_create --test.' % self.execf)

    def run_test(self):
        self.phase = "test"
        if self.spawn_child(['--only_stress', '--test'] + self.testargs()) == 0:
            raise TestFailure('%s did not crash during --only_stress --test' % self.execf)
        self.phase = "recover"
        if self.spawn_child(['--recover'] + self.prepareargs()) != 0:
            raise TestFailure('%s crashed during --recover' % self.execf)

class Worker(Thread):
    def __init__(self, scheduler):
        super(Worker, self).__init__()
        self.scheduler = scheduler

    def run(self):
        debug('%s starting.' % self)
        while not self.scheduler.stopping.isSet():
            test_runner = self.scheduler.get()
            if test_runner.is_large:
                if self.scheduler.nlarge + 1 > self.scheduler.maxlarge:
                    debug('%s pulled a large test, but there are already %d running.  Putting it back.',
                          self, self.scheduler.nlarge)
                    self.scheduler.put(test_runner)
                    continue
                self.scheduler.nlarge += 1
            try:
                test_runner.run()
            except Exception, e:
                exception('Fatal error in worker thread.')
                info('Killing all workers.')
                self.scheduler.error = e
                self.scheduler.stop()
            if test_runner.is_large:
                self.scheduler.nlarge -= 1
            if not self.scheduler.stopping.isSet():
                self.scheduler.put(test_runner)
        debug('%s exiting.' % self)

class Scheduler(Queue):
    def __init__(self, nworkers, maxlarge, logger):
        Queue.__init__(self)
        info('Initializing scheduler with %d jobs.', nworkers)
        self.nworkers = nworkers
        self.logger = logger
        self.maxlarge = maxlarge
        self.nlarge = 0  # not thread safe, don't really care right now
        self.passed = 0
        self.failed = 0
        self.workers = []
        self.stopping = Event()
        self.timer = None
        self.error = None

    def run(self, timeout):
        info('Starting workers.')
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
                debug('Scheduler interrupted.  Stopping and joining threads.')
                self.stop()
                self.join()
                sys.exit(0)
        else:
            debug('Scheduler stopped by someone else.  Joining threads.')
            self.join()

    def join(self):
        if self.timer is not None:
            self.timer.cancel()
        while len(self.workers) > 0:
            self.workers.pop().join()

    def stop(self):
        info('Stopping workers.')
        self.stopping.set()

    def __getitem__(self, k):
        return self.__dict__[k]

    def reportstr(self):
        return '[PASS=%(passed)d FAIL=%(failed)d]' % self

    def report_success(self, runner):
        self.passed += 1
        self.logger.info('PASSED %s', runner.infostr())
        info('%s PASSED %s', self.reportstr(), runner.infostr())

    def report_failure(self, runner):
        self.failed += 1
        self.logger.warning('FAILED %s', runner.infostr())
        warning('%s FAILED %s', self.reportstr(), runner.infostr())

def compiler_works(cc):
    try:
        devnull = open(os.devnull, 'w')
        r = call([cc, '-v'], stdout=devnull, stderr=STDOUT)
        devnull.close()
        return r == 0
    except OSError:
        exception('Error running %s.', cc)
        return False

def rebuild(tokudb, cc):
    env = os.environ
    env['CC'] = cc
    env['DEBUG'] = '0'
    info('Updating from svn.')
    devnull = open(os.devnull, 'w')
    call(['svn', 'up'], stdout=devnull, stderr=STDOUT, cwd=tokudb)
    devnull.close()
    if not compiler_works(cc):
        error('Cannot find working compiler named "%s".  Try sourcing the icc env script or providing another compiler with --cc.', cc)
        sys.exit(r)
    info('Building tokudb.')
    r = call(['make', '-s', 'clean'],
             cwd=tokudb, env=env)
    if r != 0:
        error('Cleaning the source tree failed.')
        sys.exit(r)
    r = call(['make', '-s', 'fastbuild'],
             cwd=tokudb, env=env)
    if r != 0:
        error('Building the fractal tree failed.')
        sys.exit(r)
    r = call(['make', '-s'] + testnames + recover_testnames,
             cwd=os.path.join(tokudb, 'src', 'tests'), env=env)
    if r != 0:
        error('Building the tests failed.')
        sys.exit(r)

def revfor(tokudb):
    proc = Popen("svn info | awk '/Revision/ {print $2}'",
                 shell=True, cwd=tokudb, stdout=PIPE)
    (out, err) = proc.communicate()
    rev = out.strip()
    info('Using tokudb at r%s.', rev)
    return rev

def main(opts):
    if opts.build:
        rebuild(opts.tokudb, opts.cc)
    rev = revfor(opts.tokudb)

    if not os.path.exists(opts.savedir):
        os.mkdir(opts.savedir)

    logger = logging.getLogger('stress')
    logger.propagate = False
    logger.setLevel(logging.INFO)
    logger.addHandler(logging.FileHandler(opts.log))

    info('Saving pass/fail logs to %s.', opts.log)
    info('Saving failure environments to %s.', opts.savedir)

    scheduler = Scheduler(opts.jobs, opts.maxlarge, logger)

    runners = []
    for tsize in [2000, 200000, 50000000]:
        for csize in [50 * tsize, 1000 ** 3]:
            for test in testnames:
                runners.append(TestRunner(scheduler, opts.tokudb, rev, opts.jemalloc,
                                          test, tsize, csize, opts.test_time, opts.savedir))
            for test in recover_testnames:
                runners.append(RecoverTestRunner(scheduler, opts.tokudb, rev, opts.jemalloc,
                                                 test, tsize, csize, opts.test_time, opts.savedir))

    shuffle(runners)

    for runner in runners:
        scheduler.put(runner)

    try:
        while scheduler.error is None:
            scheduler.run(opts.rebuild_period)
            if scheduler.error is not None:
                error('Scheduler reported an error.')
                raise scheduler.error
            rebuild(opts.tokudb, opts.cc)
            rev = revfor(opts.tokudb)
            for runner in runners:
                runner.rev = rev
    except (KeyboardInterrupt, SystemExit):
        sys.exit(0)
    except Exception, e:
        exception('Unhandled exception caught in main.')
        raise e

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
    parser.add_option('-j', '--jobs', type='int', dest='jobs', default=8,
                      help='how many concurrent tests to run [default=8]')
    parser.add_option('--maxlarge', type='int', dest='maxlarge', default=2,
                      help='maximum number of large tests to run concurrently (helps prevent swapping) [default=2]')
    parser.add_option('--no-build', action='store_false', dest='build', default=True,
                      help="don't build before testing [default=do build]")
    parser.add_option('--rebuild_period', type='int', dest='rebuild_period',
                      default=60 * 60 * 24,
                      help='how many seconds between svn up and rebuild, 0 means never rebuild [default=24 hours]')
    parser.add_option('--cc', type='string', dest='cc', default='icc',
                      help='which compiler to use [default=icc]')
    parser.add_option('--jemalloc', type='string', dest='jemalloc',
                      help='a libjemalloc.so to put in LD_PRELOAD when running tests')
    (opts, args) = parser.parse_args()
    if len(args) > 0:
        parser.print_usage()
        sys.exit(1)

    if opts.debug:
        logging.basicConfig(level=logging.DEBUG)
    elif opts.verbose:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.WARNING)

    main(opts)
