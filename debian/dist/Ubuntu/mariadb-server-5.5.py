'''apport package hook for mariadb-5.5

(c) 2009 Canonical Ltd.
Author: Mathias Gug <mathias.gug@canonical.com>
'''

import os, os.path

from apport.hookutils import *

def _add_my_conf_files(report, filename):
    key = 'MySQLConf' + path_to_key(filename)
    report[key] = ""
    for line in read_file(filename).split('\n'):
        try:
            if 'password' in line.split('=')[0]:
                line = "%s = @@APPORTREPLACED@@" % (line.split('=')[0])
            report[key] += line + '\n'
        except IndexError:
            continue

def add_info(report):
    attach_conffiles(report, 'mariadb-server-5.5', conffiles=None)
    key = 'Logs' + path_to_key('/var/log/daemon.log')
    report[key] = ""
    for line in read_file('/var/log/daemon.log').split('\n'):
        try:
            if 'mysqld' in line.split()[4]:
                report[key] += line + '\n'
        except IndexError:
            continue
    key = 'Logs' + path_to_key('/var/log/kern.log')
    report[key] = ""
    for line in read_file('/var/log/kern.log').split('\n'):
        try:
            if '/usr/sbin/mysqld' in string.join(line.split()[4:]):
                report[key] += line + '\n'
        except IndexError:
            continue
    _add_my_conf_files(report, '/etc/mysql/my.cnf')
    for f in os.listdir('/etc/mysql/conf.d'):
        _add_my_conf_files(report, os.path.join('/etc/mysql/conf.d', f))
    try:
        report['MySQLVarLibDirListing'] = unicode(os.listdir('/var/lib/mysql'))
    except OSError:
        report['MySQLVarLibDirListing'] = unicode(False)

if __name__ == '__main__':
    report = {}
    add_info(report)
    for key in report:
        print '%s: %s' % (key, report[key].split('\n', 1)[0])
