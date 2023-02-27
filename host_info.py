# Copyright (c) 2021, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
# This program is distributed in the hope that it will be useful,  but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

from typing import List, Optional, Tuple
import zipfile
import socket
import threading
import subprocess
from mysqlsh import globals, Error
import sys
import datetime
import tempfile
import os


k_host_info_cmds_macos = ["date",
                          "uname -a",
                          "mount -v",
                          "df -h",
                          "iostat -K",
                          "top -l4 -s1",
                          "ps aux",
                          "ulimit -a",
                          "dmesg",
                          "egrep -i 'err|fault|mysql' /var/log/*",
                          "netstat -ln",
                          "sysctl -a"]

k_host_info_cmds_win32 = ["date /T",
                          "ver",
                          "systeminfo",
                          "tasklist /V"]

k_host_info_cmds_linux = ["date",
                          "uname -a",
                          "getenforce",
                          "free -m",
                          "swapon -s",
                          "lsb_release -a",
                          "mount -v",
                          "df -h",
                          "cat /proc/cpuinfo",
                          "cat /proc/meminfo",
                          "cat /etc/fstab",
                          "mpstat -P ALL 1 4",
                          "iostat -m -x 1 4",
                          "vmstat 1 4",
                          "top -b -n 4 -d 1",
                          "ps aux",
                          "ulimit -a",
                          """for PID in `pidof mysqld`;do echo "# numastat -p $PID";numastat -p $PID;echo "# /proc/$PID/limits";cat /proc/$PID/limits;echo;done""",
                          "dmesg",
                          "egrep -i 'err|fault|mysql' /var/log/*",
                          "pvs", "pvdisplay",
                          "vgs", "vgdisplay",
                          "lvs", "lvdisplay",
                          "netstat -lnput",
                          "numactl --hardware",
                          "numastat -m",
                          "sysctl -a",
                          "dmidecode -s system-product-name",
                          "lsblk -i",
                          "sudo sosreport"]


def split_phases(l: List[str]) -> Tuple[List[str], List[str], List[str]]:
    before = []
    during = []
    after = []
    for s in l:
        if s.lower().startswith("before:"):
            before.append(s.partition(":")[-1])
        elif s.lower().startswith("during:"):
            during.append(s.partition(":")[-1])
        elif s.lower().startswith("after:"):
            after.append(s.partition(":")[-1])
        else:
            before.append(s)
    return before, during, after


class ShellExecutor:
    def __init__(self, custom_shell: List[str] = [], allow_phases: bool = True):
        self.custom_shell = split_phases(custom_shell)

        if not allow_phases:
            if self.custom_shell[1] or self.custom_shell[2]:
                raise Error(
                    "Option 'customShell' may not contain before:, during: or after: prefixes")

    def execute(self, zf: zipfile.ZipFile, prefix: str):
        l = self.custom_shell[0]
        if l:
            print(f"Executing custom shell scripts")
            run_shell_scripts(
                zf, f"{prefix}custom_shell.txt", l, ignore_errors=False)

    def execute_before(self, zf: zipfile.ZipFile, prefix: str):
        l = self.custom_shell[0]
        if l:
            print(f"Executing custom 'before' shell scripts")
            run_shell_scripts(
                zf, f"{prefix}custom_shell-before.txt", l, ignore_errors=False)

    def execute_during(self, zf: zipfile.ZipFile, prefix: str, iteration: int):
        l = self.custom_shell[1]
        if l:
            print(f"Executing custom 'during' iteration shell scripts")
            run_shell_scripts(
                zf, f"{prefix}custom_shell-iteration{iteration}.txt", l,
                ignore_errors=False)

    def execute_after(self, zf: zipfile.ZipFile, prefix: str):
        l = self.custom_shell[2]
        if l:
            print(f"Executing custom 'after' shell scripts")
            run_shell_scripts(
                zf, f"{prefix}custom_shell-after.txt", l, ignore_errors=False)


def dump_shell_cmd(f, cmd):
    r = subprocess.run(
        cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    f.write(f"# {cmd}\n".encode("utf-8"))
    f.write(f"# {datetime.datetime.now().isoformat()}\n".encode("utf-8"))
    f.write(f"# exitcode = {r.returncode}\n".encode("utf-8"))
    f.write(r.stdout)
    f.write(b"\n\n\n")
    return r.returncode


def run_shell_scripts(zf, fn: str, scripts: List[str], ignore_errors: bool = True):
    with zf.open(fn, "w") as f:
        for cmd in scripts:
            print(f" -> Executing {cmd}")
            rc = dump_shell_cmd(f, cmd)
            if rc != 0:
                if not ignore_errors:
                    raise Error(f'Shell command "{cmd}" exited with code {rc}')


def collect_msinfo32(zf: zipfile.ZipFile, prefix: str):
    temp_path = os.path.join(tempfile.gettempdir(), "mysqlsh_msinfo32.txt")

    args = ["msinfo32", "/report", temp_path]
    with zf.open(f"{prefix}", "w") as f:
        print("Executing", " ".join(args))
        dump_shell_cmd(f, args)

        f.write(open(temp_path, "rb").read())

    os.remove(temp_path)


def collect_host_info(zf, prefix: str):
    cmdlist = []
    if sys.platform == "darwin":
        cmdlist = k_host_info_cmds_macos
    elif sys.platform in ("win32", "cygwin", "msys"):
        cmdlist = k_host_info_cmds_win32
        collect_msinfo32(zf, prefix)
    elif sys.platform == "linux":
        cmdlist = k_host_info_cmds_linux

    print(
        f"Collecting system information for {socket.gethostname()} ({sys.platform})")
    run_shell_scripts(zf, f"{prefix}host_info",  cmdlist)


def collect_ping_stats(zf, prefix, topology_data):
    def ping(host, instance_id, timings):
        try:
            r = subprocess.run(["ping",
                                "/n" if sys.platform == "win32" else "-c", "5",
                                host], capture_output=True)
            timings.append((instance_id, host, r.stdout))
        except FileNotFoundError as e:
            timings.append((instance_id, host, str(e).encode("utf-8")))

    shell = globals.shell

    threads = []
    timings = []
    print(
        f"Executing ping from {socket.gethostname()} to other member hosts...")
    for instance in topology_data:
        endpoint = instance["endpoint"]
        instance_id = instance["instance_id"]
        host = shell.parse_uri(endpoint)["host"]

        print(f" - Executing ping {host} for 5s...")
        threads.append(threading.Thread(
            target=ping, args=(host, instance_id, timings)))
        threads[-1].start()

    for thd in threads:
        thd.join()

    with zf.open(f"{prefix}ping.txt", "w") as f:
        for instance_id, host, out in timings:
            f.write(
                f"# instance {instance_id} - ping {host}\n".encode("utf-8"))
            f.write(b"\n")
            f.write(out)
            f.write(b"\n")


if __name__ == "__main__":
    with zipfile.ZipFile("test.zip", "w") as zf:
        collect_ping_stats(
            zf, "test", [{"endpoint": "localhost"}, {"endpoint": "www.oracle.com"}])
