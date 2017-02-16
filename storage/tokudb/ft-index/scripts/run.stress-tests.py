#!/usr/bin/env python
"""
A script for running our stress tests repeatedly to see if any fail.

Runs a list of stress tests in parallel, reporting passes and collecting
failure scenarios until killed.  Runs with different table sizes,
cachetable sizes, and numbers of threads.

Suitable for running on a dev branch, or a release branch, or main.

Just run the script from within a branch you want to test.

By default, we stop everything, update from git, rebuild, and restart the
tests once a day.
"""

import logging
import os
import re
import stat
import sys
import time

from email.mime.text import MIMEText
from glob import glob
from logging import debug, info, warning, error, exception
from optparse import OptionGroup, OptionParser
from Queue import Queue
from random import randrange, shuffle
from resource import setrlimit, RLIMIT_CORE
from shutil import copy, copytree, move, rmtree
from signal import signal, SIGHUP, SIGINT, SIGPIPE, SIGALRM, SIGTERM
from smtplib import SMTP
from socket import gethostname
from subprocess import call, Popen, PIPE, STDOUT
from traceback import format_exc
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

def setlimits():
    setrlimit(RLIMIT_CORE, (-1, -1))
    os.nice(7)

class TestFailure(Exception):
    pass

class Killed(Exception):
    pass

class TestRunnerBase(object):
    def __init__(self, scheduler, builddir, rev, execf, tsize, csize, default_test_time, savedir):
        self.scheduler = scheduler
        self.builddir = builddir
        self.rev = rev
        self.execf = execf
        self.tsize = tsize
        self.csize = csize
        self.default_test_time = default_test_time
        self.long_test_index = randrange(16)
        self.savedir = savedir

        self.env = os.environ

        self.nruns = 0
        self.num_ptquery = 1
        self.num_update = 1
        self.rundir = None
        self.outf = None
        self.times = [0, 0]
        self.is_large = (tsize >= 10000000)
        self.oldversionstr = 'noupgrade'

    def __str__(self):
        return (self.__class__.__name__ +
                '<%(execf)s, %(tsize)d, %(csize)d, %(oldversionstr)s>') % self

    def __getitem__(self, k):
        return self.__getattribute__(k)

    def infostr(self):
        return '\t'.join(['%(execf)s',
                          '%(rev)s',
                          '%(tsize)d',
                          '%(csize)d',
                          '%(oldversionstr)s',
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
    def test_time(self):
        if self.nruns % 16 == self.long_test_index:
            return 3600
        else:
            return self.default_test_time

    @property
    def envdir(self):
        return os.path.join(self.rundir, 'envdir')

    @property
    def prepareloc(self):
        preparename = 'dir.%(execf)s-%(tsize)d-%(csize)d' % self
        return os.path.join(self.builddir, 'src', 'tests', preparename)

    def prepare(self):
        if os.path.isdir(self.prepareloc):
            debug('%s found existing environment.', self)
            copytree(self.prepareloc, self.envdir)
        else:
            debug('%s preparing an environment.', self)
            self.run_prepare()
            self.save_prepared_envdir()

    def save_prepared_envdir(self):
        debug('%s copying environment to %s.', self, self.prepareloc)
        copytree(self.envdir, self.prepareloc)

    def run(self):
        if self.nruns % 2 < 1:
            self.num_ptquery = 1
        else:
            self.num_ptquery = randrange(16)
        if self.nruns % 4 < 2:
            self.num_update = 1
        else:
            self.num_update = randrange(16)

        srctests = os.path.join(self.builddir, 'src', 'tests')
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
                self.times[1] = time.time()
                savepfx = '%(execf)s-%(rev)s-%(tsize)d-%(csize)d-%(num_ptquery)d-%(num_update)d-%(phase)s-' % self
                if not os.path.exists(self.savedir):
                    os.mkdir(self.savedir)
                savedir = mkdtemp(dir=self.savedir, prefix=savepfx)
                tarfile = '%s.tar' % savedir
                commands = ''
                try:
                    f = open(os.path.join(self.rundir, 'commands.txt'))
                    commands = f.read()
                    f.close()
                except:
                    pass
                output = ''
                try:
                    f = open(os.path.join(self.rundir, 'output.txt'))
                    output = f.read()
                    f.close()
                except:
                    pass
                self.scheduler.email_failure(self, tarfile, commands, output)
                self.save(savedir, tarfile)
                self.scheduler.report_failure(self)
                warning('Saved environment to %s', tarfile)
            else:
                self.scheduler.report_success(self)
        finally:
            self.outf.close()
            rmtree(self.rundir)
            self.rundir = None
            self.times = [0, 0]
            self.nruns += 1

    def save(self, savedir, tarfile):
        def targetfor(path):
            return os.path.join(savedir, os.path.basename(path))

        for f in glob(os.path.join(self.rundir, '*')):
            if os.path.isdir(f):
                copytree(f, targetfor(f))
            else:
                copy(f, targetfor(f))
        fullexecf = os.path.join(self.builddir, 'src', 'tests', self.execf)
        copy(fullexecf, targetfor(fullexecf))

        # TODO: Leif was lazy and did this in bash, it should be done in python for portability
        os.system("for l in $(ldd %(fullexecf)s | sed 's/\ *(0x[0-9a-f]*)$//;s/.*=>\ \?//;s/^\ *|\ *$//' | grep -v '^$'); do mkdir -p %(savedir)s/$(dirname $l); cp $l %(savedir)s/$l; done" % {'fullexecf': fullexecf, 'savedir': savedir})

        r = call(['tar', 'cf', os.path.basename(tarfile), os.path.basename(savedir)], cwd=os.path.dirname(savedir))
        if r != 0:
            error('tarring up %s failed.' % savedir)
            sys.exit(r)
        os.chmod(tarfile, stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH)

    def waitfor(self, proc):
        while proc.poll() is None:
            self.scheduler.stopping.wait(1)
            if self.scheduler.stopping.isSet():
                os.kill(proc.pid, SIGTERM)
                raise Killed()

    def spawn_child(self, args):
        logging.debug('%s spawning %s', self, ' '.join([self.execf] + args))
        commandsf = open(os.path.join(self.rundir, 'commands.txt'), 'a')
        print >>commandsf, ' '.join([self.execf] + args)
        commandsf.close()
        proc = Popen([self.execf] + args,
                     executable=os.path.join('..', self.execf),
                     env=self.env,
                     cwd=self.rundir,
                     preexec_fn=setlimits,
                     stdout=self.outf,
                     stderr=STDOUT)
        self.waitfor(proc)
        return proc.returncode

    @property
    def extraargs(self):
        # for overriding
        return []

    @property
    def prepareargs(self):
        return ['-v',
                '--envdir', 'envdir',
                '--num_elements', str(self.tsize),
                '--cachetable_size', str(self.csize)] + self.extraargs

    @property
    def testargs(self):
        return ['--num_seconds', str(self.test_time),
                '--join_timeout', str(60 * 60 * 12),
                '--no-crash_on_operation_failure',
                '--num_ptquery_threads', str(self.num_ptquery),
                '--num_update_threads', str(self.num_update)] + self.prepareargs

class TestRunner(TestRunnerBase):
    def run_prepare(self):
        self.phase = "create"
        if self.spawn_child(['--only_create'] + self.prepareargs) != 0:
            raise TestFailure('%s crashed during --only_create.' % self.execf)

    def run_test(self):
        self.phase = "stress"
        if self.spawn_child(['--only_stress'] + self.testargs) != 0:
            raise TestFailure('%s crashed during --only_stress.' % self.execf)

class RecoverTestRunner(TestRunnerBase):
    def run_prepare(self):
        self.phase = "create"
        if self.spawn_child(['--only_create', '--test'] + self.prepareargs) != 0:
            raise TestFailure('%s crashed during --only_create --test.' % self.execf)

    def run_test(self):
        self.phase = "test"
        if self.spawn_child(['--only_stress', '--test'] + self.testargs) == 0:
            raise TestFailure('%s did not crash during --only_stress --test' % self.execf)
        self.phase = "recover"
        if self.spawn_child(['--recover'] + self.prepareargs) != 0:
            raise TestFailure('%s crashed during --recover' % self.execf)

class UpgradeTestRunnerMixin(TestRunnerBase):
    def __init__(self, old_environments_dir, version, pristine_or_stressed, **kwargs):
        super(UpgradeTestRunnerMixin, self).__init__(**kwargs)
        self.version = version
        self.pristine_or_stressed = pristine_or_stressed
        self.old_env_dirs = os.path.join(old_environments_dir, version)
        self.oldversionstr = '%(version)s-%(pristine_or_stressed)s' % self

    @property
    def extraargs(self):
        return ['--num_DBs', '1']

    @property
    def old_envdir(self):
        oldname = 'saved%(pristine_or_stressed)s-%(tsize)d-dir' % self
        logging.debug('%s using old version environment %s from %s.', self, oldname, self.old_env_dirs)
        return os.path.join(self.old_env_dirs, oldname)

    def save_prepared_envdir(self):
        # no need to do this
        pass

    def run_prepare(self):
        self.phase = "create"
        copytree(self.old_envdir, self.envdir)

class DoubleTestRunnerMixin(TestRunnerBase):
    """Runs the test phase twice in a row.

    Good for upgrade tests, to run the test once to upgrade it and then
    again to make sure the upgrade left it in a good state.
    """

    def run_test(self):
        super(DoubleTestRunnerMixin, self).run_test()
        super(DoubleTestRunnerMixin, self).run_test()

class UpgradeTestRunner(UpgradeTestRunnerMixin, TestRunner):
    pass

class UpgradeRecoverTestRunner(UpgradeTestRunnerMixin, RecoverTestRunner):
    pass

class DoubleUpgradeTestRunner(DoubleTestRunnerMixin, UpgradeTestRunner):
    pass

class DoubleUpgradeRecoverTestRunner(DoubleTestRunnerMixin, UpgradeRecoverTestRunner):
    pass

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
                self.scheduler.error = format_exc()
                self.scheduler.stop()
            if test_runner.is_large:
                self.scheduler.nlarge -= 1
            if not self.scheduler.stopping.isSet():
                self.scheduler.put(test_runner)
        debug('%s exiting.' % self)

class Scheduler(Queue):
    def __init__(self, nworkers, maxlarge, logger, email, branch):
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
        self.email = email
        self.branch = branch

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
            if self.error:
                send_mail(self.email, 'Stress tests scheduler stopped by something, on %s' % gethostname(), self.error)
                sys.exit(77)

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

    def email_failure(self, runner, savedtarfile, commands, output):
        if self.email is None:
            return

        h = gethostname()
        if isinstance(runner, UpgradeTestRunnerMixin):
            upgradestr = '''
The test was upgrading from %s.''' % runner.oldversionstr
        else:
            upgradestr = ''
        send_mail(self.email,
                  'Stress test failure on %(hostname)s running %(branch)s.' % { 'hostname': h, 'branch': self.branch },
                  ('''A stress test failed on %(hostname)s running %(branch)s at revision %(rev)s after %(test_duration)d seconds.%(upgradestr)s
Its environment is saved to %(tarfile)s on that machine.

The test configuration was:

testname:            %(execf)s
num_elements:        %(tsize)d
cachetable_size:     %(csize)d
num_ptquery_threads: %(num_ptquery)d
num_update_threads:  %(num_update)d

Commands run:
%(commands)s

Test output:
%(output)s
''' % {
                    'hostname': h,
                    'rev': runner.rev,
                    'test_duration': runner.time,
                    'upgradestr': upgradestr,
                    'tarfile': savedtarfile,
                    'execf': runner.execf,
                    'tsize': runner.tsize,
                    'csize': runner.csize,
                    'num_ptquery': runner.num_ptquery,
                    'num_update': runner.num_update,
                    'branch': self.branch,
                    'commands': commands,
                    'output': output,
                    }))

def send_mail(toaddrs, subject, body):
    m = MIMEText(body)
    fromaddr = 'tim@tokutek.com'
    m['From'] = fromaddr
    m['To'] = ', '.join(toaddrs)
    m['Subject'] = subject
    s = SMTP('192.168.1.114')
    s.sendmail(fromaddr, toaddrs, str(m))
    s.quit()

def update(tokudb):
    info('Updating from git.')
    devnull = open(os.devnull, 'w')
    call(['git', 'pull'], stdout=devnull, stderr=STDOUT, cwd=tokudb)
    devnull.close()

def rebuild(tokudb, builddir, tokudb_data, cc, cxx, tests):
    info('Building tokudb.')
    if not os.path.exists(builddir):
        os.mkdir(builddir)
    newenv = os.environ
    newenv['CC'] = cc
    newenv['CXX'] = cxx
    r = call(['cmake',
              '-DCMAKE_BUILD_TYPE=Debug',
              '-DUSE_GTAGS=OFF',
              '-DUSE_CTAGS=OFF',
              '-DUSE_ETAGS=OFF',
              '-DUSE_CSCOPE=OFF',
              '-DTOKUDB_DATA=%s' % tokudb_data,
              tokudb],
             env=newenv,
             cwd=builddir)
    if r != 0:
        send_mail(['leif@tokutek.com'], 'Stress tests on %s failed to build.' % gethostname(), '')
        error('Building the tests failed.')
        sys.exit(r)
    r = call(['make', '-j8'], cwd=builddir)
    if r != 0:
        send_mail(['leif@tokutek.com'], 'Stress tests on %s failed to build.' % gethostname(), '')
        error('Building the tests failed.')
        sys.exit(r)

def revfor(tokudb):
    proc = Popen("git describe --tags",
                 shell=True, cwd=tokudb, stdout=PIPE)
    (out, err) = proc.communicate()
    rev = out.strip()
    info('Using tokudb at r%s.', rev)
    return rev

def main(opts):
    builddir = os.path.join(opts.tokudb, 'build')
    if opts.build:
        rebuild(opts.tokudb, builddir, opts.tokudb_data, opts.cc, opts.cxx, opts.testnames + opts.recover_testnames)
    rev = revfor(opts.tokudb)

    if not os.path.exists(opts.savedir):
        os.mkdir(opts.savedir)

    logger = logging.getLogger('stress')
    logger.propagate = False
    logger.setLevel(logging.INFO)
    logger.addHandler(logging.FileHandler(opts.log))

    info('Saving pass/fail logs to %s.', opts.log)
    info('Saving failure environments to %s.', opts.savedir)

    scheduler = Scheduler(opts.jobs, opts.maxlarge, logger, opts.email, opts.branch)

    runners = []
    for tsize in [2000, 200000, 50000000]:
        for csize in [50 * tsize, 1000 ** 3]:
            kwargs = {
                'scheduler': scheduler,
                'builddir': builddir,
                'rev': rev,
                'tsize': tsize,
                'csize': csize,
                'default_test_time': opts.test_time,
                'savedir': opts.savedir
                }
            for test in opts.testnames:
                if opts.run_non_upgrade:
                    runners.append(TestRunner(execf=test, **kwargs))

                # never run test_stress_openclose.tdb on existing
                # environments, it doesn't want them
                if opts.run_upgrade and test != 'test_stress_openclose.tdb':
                    for version in opts.old_versions:
                        for pristine_or_stressed in ['pristine', 'stressed']:
                            upgrade_kwargs = {
                                'old_environments_dir': opts.old_environments_dir,
                                'version': version,
                                'pristine_or_stressed': pristine_or_stressed
                                }
                            upgrade_kwargs.update(kwargs)
                            # skip running test_stress4.tdb on any env
                            # that has already been stressed, as that
                            # breaks its assumptions
                            if opts.double_upgrade and test != 'test_stress4.tdb':
                                runners.append(DoubleUpgradeTestRunner(
                                        execf=test,
                                        **upgrade_kwargs))
                            elif not (test == 'test_stress4.tdb' and pristine_or_stressed == 'stressed'):
                                runners.append(UpgradeTestRunner(
                                        execf=test,
                                        **upgrade_kwargs))

            for test in opts.recover_testnames:
                if opts.run_non_upgrade:
                    runners.append(RecoverTestRunner(execf=test, **kwargs))

                if opts.run_upgrade:
                    for version in opts.old_versions:
                        for pristine_or_stressed in ['pristine', 'stressed']:
                            upgrade_kwargs = {
                                'old_environments_dir': opts.old_environments_dir,
                                'version': version,
                                'pristine_or_stressed': pristine_or_stressed
                                }
                            upgrade_kwargs.update(kwargs)
                            if opts.double_upgrade:
                                runners.append(DoubleUpgradeRecoverTestRunner(
                                        execf=test,
                                        **upgrade_kwargs))
                            else:
                                runners.append(UpgradeRecoverTestRunner(
                                        execf=test,
                                        **upgrade_kwargs))

    shuffle(runners)

    for runner in runners:
        scheduler.put(runner)

    try:
        while scheduler.error is None:
            scheduler.run(opts.rebuild_period)
            if scheduler.error is not None:
                error('Scheduler reported an error.')
                raise scheduler.error
            update(opts.tokudb)
            rebuild(opts.tokudb, builddir, opts.tokudb_data, opts.cc, opts.cxx, opts.testnames + opts.recover_testnames)
            rev = revfor(opts.tokudb)
            for runner in runners:
                runner.rev = rev
    except (KeyboardInterrupt, SystemExit):
        sys.exit(0)
    except Exception, e:
        exception('Unhandled exception caught in main.')
        send_mail(['leif@tokutek.com'], 'Stress tests caught unhandled exception in main, on %s' % gethostname(), format_exc())
        raise e

if __name__ == '__main__':
    a0 = os.path.abspath(sys.argv[0])
    usage = '%prog [options]\n' + __doc__
    parser = OptionParser(usage=usage)
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose', default=False, help='show build status, passing tests, and other info')
    parser.add_option('-d', '--debug', action='store_true', dest='debug', default=False, help='show debugging info')
    parser.add_option('-l', '--log', type='string', dest='log',
                      default='/tmp/run.stress-tests.log',
                      help='where to save logfiles')
    parser.add_option('-s', '--savedir', type='string', dest='savedir',
                      default='/tmp/run.stress-tests.failures',
                      help='where to save environments and extra data for failed tests')
    parser.add_option('--email', action='append', type='string', dest='email', default=[], help='where to send emails')
    parser.add_option('--no-email', action='store_false', dest='send_emails', default=True, help='suppress emails on failure')
    default_toplevel = os.path.dirname(os.path.dirname(a0))
    parser.add_option('--tokudb', type='string', dest='tokudb',
                      default=default_toplevel,
                      help=('top of the tokudb tree (contains ft/ and src/) [default=%s]' % os.path.relpath(default_toplevel)))
    toplevel_basename = os.path.basename(default_toplevel)
    if toplevel_basename == 'tokudb':
        maybe_absolute_branchpath = os.path.dirname(default_toplevel)
        if os.path.basename(os.path.dirname(maybe_absolute_branchpath)) == 'mysql.branches':
            default_branchname = os.path.basename(maybe_absolute_branchpath)
        else:
            default_branchname = 'mainline'
    elif toplevel_basename[:7] == 'tokudb.':
        default_branchname = toplevel_basename[7:]
    else:
        default_branchname = 'unknown branch'
    parser.add_option('--branch', type='string', dest='branch',
                      default=default_branchname,
                      help=('what to call this branch [default=%s]' % default_branchname))

    test_group = OptionGroup(parser, 'Scheduler Options', 'Control how the scheduler runs jobs.')
    test_group.add_option('-t', '--test_time', type='int', dest='test_time',
                          default=60,
                          help='time to run each test, in seconds [default=60]'),
    test_group.add_option('-j', '--jobs', type='int', dest='jobs', default=8,
                          help='how many concurrent tests to run [default=8]')
    test_group.add_option('--maxlarge', type='int', dest='maxlarge', default=2,
                          help='maximum number of large tests to run concurrently (helps prevent swapping) [default=2]')
    parser.add_option_group(test_group)


    default_testnames = ['test_stress0.tdb',
                         'test_stress1.tdb',
                         'test_stress2.tdb',
                         'test_stress3.tdb',
                         'test_stress4.tdb',
                         'test_stress5.tdb',
                         'test_stress6.tdb',
                         'test_stress7.tdb',
                         'test_stress_hot_indexing.tdb',
                         'test_stress_with_verify.tdb',
                         'test_stress_openclose.tdb']
    default_recover_testnames = ['recover-test_stress1.tdb',
                                 'recover-test_stress2.tdb',
                                 'recover-test_stress3.tdb',
                                 'recover-child-rollback.tdb',
                                 'recover-test_stress_openclose.tdb']
    build_group = OptionGroup(parser, 'Build Options', 'Control how the fractal tree and tests get built.')
    build_group.add_option('--skip_build', action='store_false', dest='build', default=True,
                           help='skip the git pull and build phase before testing [default=False]')
    build_group.add_option('--rebuild_period', type='int', dest='rebuild_period', default=60 * 60 * 24,
                           help='how many seconds between doing an git pull and rebuild, 0 means never rebuild [default=24 hours]')
    default_tokudb_data = os.path.abspath(os.path.join(default_toplevel, '..', 'tokudb.data'))
    build_group.add_option('--tokudb_data', type='string', dest='tokudb_data', default=default_tokudb_data,
                           help='passed to cmake as TOKUDB_DATA [default=%s]' % default_tokudb_data)
    build_group.add_option('--cc', type='string', dest='cc', default='gcc47',
                           help='which compiler to use [default=gcc47]')
    build_group.add_option('--cxx', type='string', dest='cxx', default='g++47',
                           help='which compiler to use [default=g++47]')
    build_group.add_option('--add_test', action='append', type='string', dest='testnames', default=default_testnames,
                           help=('add a stress test to run [default=%r]' % default_testnames))
    build_group.add_option('--add_recover_test', action='append', type='string', dest='recover_testnames', default=default_recover_testnames,
                           help=('add a recover stress test to run [default=%r]' % default_recover_testnames))
    parser.add_option_group(build_group)

    upgrade_group = OptionGroup(parser, 'Upgrade Options', 'Also run on environments from old versions of tokudb.')
    upgrade_group.add_option('--run_upgrade', action='store_true', dest='run_upgrade', default=False,
                             help='run the tests on old dictionaries as well, to test upgrade [default=False]')
    upgrade_group.add_option('--skip_non_upgrade', action='store_false', dest='run_non_upgrade', default=True,
                             help="skip the tests that don't involve upgrade [default=False]")
    upgrade_group.add_option('--double_upgrade', action='store_true', dest='double_upgrade', default=False,
                             help='run the upgrade tests twice in a row [default=False]')
    upgrade_group.add_option('--add_old_version', action='append', type='choice', dest='old_versions', choices=['4.2.0', '5.0.8', '5.2.7', '6.0.0', '6.1.0', '6.5.1', '6.6.3', '7.0.1','7.1.6','v26','7.5.0'],
                             help='which old versions to use for running the stress tests in upgrade mode. can be specified multiple times [options=4.2.0, 5.0.8, 5.2.7, 6.0.0, 6.1.0, 6.5.1, 6.6.3, 7.1.0, 7.1.6, v26, 7.5.0]')
    upgrade_group.add_option('--old_environments_dir', type='string', dest='old_environments_dir',
                             default=('%s/old-stress-test-envs' % default_tokudb_data),
                             help='directory containing old version environments (should contain 5.0.8/, 5.2.7/, etc, and the environments should be in those) [default=../../tokudb.data/stress_environments]')
    parser.add_option_group(upgrade_group)

    (opts, args) = parser.parse_args()
    if len(args) > 0:
        parser.error('Invalid arguments: %r' % args)

    if opts.old_versions is not None and len(opts.old_versions) > 0:
        opts.run_upgrade = True

    if opts.run_upgrade:
        if not os.path.isdir(opts.old_environments_dir):
            parser.error('You specified --run_upgrade but did not specify an --old_environments_dir that exists.')
        if len(opts.old_versions) < 1:
            parser.error('You specified --run_upgrade but gave no --old_versions to run against.')
        for version in opts.old_versions:
            version_dir = os.path.join(opts.old_environments_dir, version)
            if not os.path.isdir(version_dir):
                parser.error('You specified --run_upgrade but %s is not a directory.' % version_dir)

    if not opts.send_emails:
        opts.email = None
    elif len(opts.email) == 0:
        opts.email.append('tokueng@tokutek.com')

    if opts.debug:
        logging.basicConfig(level=logging.DEBUG)
    elif opts.verbose:
        logging.basicConfig(level=logging.INFO)
    else:
        logging.basicConfig(level=logging.WARNING)

    main(opts)
