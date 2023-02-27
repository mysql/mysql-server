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

from multiprocessing import Condition
from threading import Thread
from mysqlsh import mysql, Error
from typing import Callable, List, Optional, Tuple
import yaml
import json
import datetime
import zipfile

from .host_info import ShellExecutor, split_phases


def make_zipinfo(path: str) -> zipfile.ZipInfo:
    return zipfile.ZipInfo(path,
                           date_time=(tuple(
                               datetime.datetime.now().timetuple())))


def sanitize(s):
    return s.replace(' ', '_').replace(':',
                                       '_').replace('\\',
                                                    '_').replace('/', '_')


def format_bytes(n):
    if n is None:
        return None
    if n < 1024:
        return f"{int(n)} bytes"
    elif n < 1024 * 1024:
        return f"{n/1024:0.2f} KiB"
    elif n < 1024 * 1024 * 1024:
        return f"{n/(1024*1024):0.2f} MiB"
    elif n < 1024 * 1024 * 1024 * 1024:
        return f"{n/(1024*1024*1024):0.2f} GiB"
    else:
        return f"{n/(1024*1024*1024*1024):0.2f} TiB"


def format_pico_time(picoseconds):
    if picoseconds is None:
        return None
    elif picoseconds >= 604800000000000000:
        return f'{picoseconds / 604800000000000000:0.2f} w'
    elif picoseconds >= 86400000000000000:
        return f'{picoseconds / 86400000000000000:0.2f} d'
    elif picoseconds >= 3600000000000000:
        return f'{picoseconds / 3600000000000000:0.2f} h'
    elif picoseconds >= 60000000000000:
        return f'{picoseconds / 60000000000000:0.2f} m'
    elif picoseconds >= 1000000000000:
        return f'{picoseconds / 1000000000000:0.2f} s'
    elif picoseconds >= 1000000000:
        return f'{picoseconds / 1000000000:0.2f} ms'
    elif picoseconds >= 1000000:
        return f'{picoseconds / 1000000:0.2f} us'
    elif picoseconds >= 1000:
        return f'{picoseconds / 1000:0.2f} ns'
    else:
        return f'{picoseconds} ps'


class InstanceSession:

    def __init__(self, session):
        self.session = session

        self.has_pfs, version = self.run_sql(
            "select @@performance_schema <> 'OFF', @@version").fetch_one()
        if "-" in version:
            version = version.split("-")[0]
        a, b, c = version.split(".")
        self.version = int(a) * 10000 + int(b) * 100 + int(c)

        self.pfs_tables = [
            t[0] for t in self.run_sql(
                "show tables in performance_schema").fetch_all()
        ]
        self.is_tables = [
            t[0] for t in self.run_sql(
                "show tables in information_schema").fetch_all()
        ]
        self.sys_tables = [
            r[0] for r in self.run_sql("show tables in sys").fetch_all()
        ]
        self.mysql_tables = [
            r[0] for r in self.run_sql("show tables in mysql").fetch_all()
        ]

        supported_engines = [
            r[0] for r in self.run_sql(
                "select engine from information_schema.engines where support<>'NO'"
            ).fetch_all()
        ]

        self.has_ndb = "NDBCluster" in supported_engines
        self.has_rapid = "rpd_nodes" in self.pfs_tables

        if self.session.run_sql(
                "show schemas like 'mysql_innodb_cluster_metadata'").fetch_one(
        ):
            self.has_innodbcluster = True
            try:
                self.instance_id = self.session.run_sql(
                    "select instance_id from mysql_innodb_cluster_metadata.v2_this_instance").fetch_one()[0]
            except:
                self.instance_id = self.session.run_sql(
                    "select instance_id from mysql_innodb_cluster_metadata.instances where cast(mysql_server_uuid as binary)=cast(@@server_uuid as binary)"
                ).fetch_one()[0]
        else:
            self.has_innodbcluster = False
            self.instance_id = 0

    def __str__(self):
        return self.session.uri

    @property
    def uri(self) -> str:
        return self.session.uri

    def run_sql(self, sql: str, args: list = []):
        try:
            return self.session.run_sql(sql, args)
        except Exception as e:
            print("ERROR running query: ", sql, str(e))
            raise


def write_tsv(zf: zipfile.ZipFile, fn: str, tsv_out: list, header: str = ""):

    def write(f):
        if header:
            f.write(header.encode("utf-8"))
        for line in tsv_out:
            f.write(line.encode("utf-8"))
            f.write(b"\n")

    with zf.open(make_zipinfo(fn + ".tsv"), "w") as f:
        write(f)


def dump_query(zf: zipfile.ZipFile,
               fn: str,
               session: InstanceSession,
               query: str,
               args: List[str] = [],
               *,
               as_yaml: bool = True,
               as_tsv: bool = True,
               filter: Optional[Callable] = None,
               formatters: List[Callable] = [],
               ignore_errors: bool = True,
               include_warnings: bool = False) -> list:

    def handle_error(e):
        if ignore_errors:
            with zf.open(make_zipinfo(fn + ".error"), "w") as f:
                f.write(f"{header}\n# {e}\n".encode("utf-8"))
            return []
        print(f'ERROR: While executing "{query}": {e}')
        raise

    header = "# Query:\n" + \
        "\n".join([f"#\t{l}" for l in query.split("\n")]) + "\n"
    header += "#\n"
    header += f"# Started: {datetime.datetime.now().isoformat()}\n"
    try:
        r = session.run_sql(query, args)
        execution_time = r.get_execution_time()
    except Exception as e:
        return handle_error(e)

    raw_out = []

    yaml_out = []
    tsv_out = []

    if as_tsv:
        line = []
        for c in r.columns:
            line.append(c.column_label)
        tsv_out.append("# " + "\t".join(line))

    try:
        for row in iter(r.fetch_one, None):
            line = []
            if filter:
                row = filter(row)
                if not row:
                    continue
            entry = {}
            raw_entry = {}
            for i in range(len(row)):
                field = row[i]
                raw_entry[r.columns[i].column_label] = field
                if r.columns[i].column_label in formatters:
                    field = formatters[i](field)
                line.append(str(field) if field is not None else "NULL")
                if type(field) == str:
                    if '"' in field:
                        field = '"' + field.replace('"', '\\"') + '"'
                    entry[r.columns[i].column_label] = field
                elif type(field) in (int, str, bool, type(None), float):
                    entry[r.columns[i].column_label] = field
                else:
                    entry[r.columns[i].column_label] = str(field)
            if as_tsv:
                tsv_out.append("\t".join(line))
            yaml_out.append(entry)
            raw_out.append(raw_entry)
    except Exception as e:
        return handle_error(e)

    if include_warnings:
        warnings = session.run_sql("SHOW WARNINGS").fetch_all()
        if warnings:
            yaml_out.append({"Warnings": warnings})
            if as_tsv:
                tsv_out.append("# Warnings")
                for w in warnings:
                    tsv_out.append("\t".join([str(f) for f in w]))

    header += f"# Execution Time: {execution_time}\n#\n"

    if as_tsv:
        write_tsv(zf, fn, tsv_out, header=header)

    if as_yaml:
        with zf.open(make_zipinfo(fn + ".yaml"), "w") as f:
            f.write(header.encode("utf-8"))
            f.write(yaml.dump_all(yaml_out).encode("utf-8"))

    return raw_out


def dump_table(zf: zipfile.ZipFile,
               fn: str,
               session: InstanceSession,
               table,
               *,
               as_yaml=False,
               filter=None,
               ignore_errors=True):
    return dump_query(zf,
                      fn,
                      session,
                      f"select * from {table}",
                      filter=filter,
                      as_yaml=as_yaml,
                      ignore_errors=ignore_errors)


def collect_tables(zf: zipfile.ZipFile,
                   prefix: str,
                   session: InstanceSession,
                   tables: List[str],
                   *,
                   as_yaml: bool = False,
                   ignore_errors: bool = True) -> dict:
    info = {}
    for table in tables:
        print(f" - Gathering {table}...")
        info[table] = dump_table(zf,
                                 f"{prefix}{sanitize(table)}",
                                 session,
                                 table,
                                 as_yaml=as_yaml,
                                 ignore_errors=ignore_errors)
    return info


def collect_queries(zf: zipfile.ZipFile,
                    prefix: str,
                    session: InstanceSession,
                    queries: list,
                    *,
                    as_yaml=False,
                    ignore_errors: bool = True,
                    include_warnings: bool = False):
    info = {}
    for q in queries:
        if type(q) is tuple:
            if len(q) == 2:
                label, query = q
                filter = None
            else:
                label, query, filter = q
        else:
            label, query, filter = q, q, None
        print(f" - Gathering {label}...")
        try:
            info[label.replace(' ', '_')] = dump_query(
                zf,
                f"{prefix}{sanitize(label)}",
                session,
                query,
                as_yaml=as_yaml,
                ignore_errors=ignore_errors,
                include_warnings=include_warnings,
                filter=filter)
        except Error as e:
            if e.code not in (mysql.ErrorCode.ER_NO_BINARY_LOGGING, ):
                raise
    return info


def collect_queries_single_file(zf: zipfile.ZipFile, fn: str,
                                session: InstanceSession, queries: list):
    with zf.open(make_zipinfo(fn), "w") as f:
        for query in queries:
            print(f" - Executing {query}...")

            header = "# Query:\n" + \
                "\n".join([f"#\t{l}" for l in query.split("\n")]) + "\n"
            f.write(header.encode("utf-8"))
            try:
                res = session.run_sql(query)
                f.write(
                    f"# Execution Time: {res.get_execution_time()}\n\n".encode(
                        "utf-8"))
                f.write(b"\n")
                f.write(
                    ("# " + "\t".join([c.column_label for c in res.columns]) +
                     "\n").encode("utf-8"))

                line = []
                for row in iter(res.fetch_one, None):
                    for i in range(len(row)):
                        field = row[i]
                        line.append(
                            str(field) if field is not None else "NULL")
                line = "\t".join(line)
                f.write(f"{line}\n".encode("utf-8"))
            except Error as e:
                f.write(f"# Error: {e}\n".encode("utf-8"))
                print(f"ERROR: {e}")


k_sys_views_delta = [
    # TABLE_NAME, order_by, order_by_delta, where_delta, limit_rows, pk
    ('host_summary', '%{TABLE}.statement_latency DESC',
     lambda s, e: (e["statement_latency"]-(s["statement_latency"] or 0)),
     lambda s, e: e["statements"] - (s["statements"] or 0) > 0, None, 'host'),
    ('host_summary_by_file_io', '%{TABLE}.io_latency DESC',
     '(e.io_latency-IFNULL(s.io_latency, 0)) DESC',
     lambda s, e: (e["ios"] - (s["ios"] or 0)) > 0, None, 'host'),
    ('host_summary_by_file_io_type', '%{TABLE}.host, %{TABLE}.total_latency DESC',
     'e.host, (e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'host,event_name'),
    ('host_summary_by_stages', '%{TABLE}.host, %{TABLE}.total_latency DESC',
     'e.host, (e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'host,event_name'),
    ('host_summary_by_statement_latency', '%{TABLE}.total_latency DESC',
     '(e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'host'),
    ('host_summary_by_statement_type', '%{TABLE}.host, %{TABLE}.total_latency DESC',
     'e.host, (e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'host,statement'),
    ('io_by_thread_by_latency', '%{TABLE}.total_latency DESC',
     '(e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'user,thread_id,processlist_id'),
    ('io_global_by_file_by_bytes', '%{TABLE}.total DESC',
     '(e.total-IFNULL(s.total, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, 100, 'file'),
    ('io_global_by_file_by_latency', '%{TABLE}.total_latency DESC',
     '(e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, 100, 'file'),
    ('io_global_by_wait_by_bytes', '%{TABLE}.total_requested DESC',
     '(e.total_requested-IFNULL(s.total_requested, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'event_name'),
    ('io_global_by_wait_by_latency', '%{TABLE}.total_latency DESC',
     '(e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'event_name'),
    ('schema_index_statistics', '(%{TABLE}.select_latency+%{TABLE}.insert_latency+%{TABLE}.update_latency+%{TABLE}.delete_latency) DESC',
     '((e.select_latency+e.insert_latency+e.update_latency+e.delete_latency)-IFNULL(s.select_latency+s.insert_latency+s.update_latency+s.delete_latency, 0)) DESC',
     lambda s, e: ((e["rows_selected"]+e["insert_latency"]+e["rows_updated"]+e["rows_deleted"]) - \
                   (s["rows_selected"]+s["rows_inserted"]+s["rows_updated"]+s["rows_deleted"] or 0)) > 0,
     100, 'table_schema,table_name,index_name'),
    ('schema_table_statistics', '%{TABLE}.total_latency DESC',
     '(e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total_latency"]-(s["total_latency"] or 0)) > 0, 100, 'table_schema,table_name'),
    ('schema_tables_with_full_table_scans', '%{TABLE}.rows_full_scanned DESC',
     '(e.rows_full_scanned-IFNULL(s.rows_full_scanned, 0)) DESC',
     lambda s, e: (e["rows_full_scanned"]-(s["rows_full_scanned"] or 0)) > 0, 100, 'object_schema,object_name'),
    ('user_summary', '%{TABLE}.statement_latency DESC',
     '(e.statement_latency-IFNULL(s.statement_latency, 0)) DESC',
     lambda s, e: (e["statements"] - (s["statements"] or 0)) > 0, None, 'user'),
    ('user_summary_by_file_io', '%{TABLE}.io_latency DESC',
     '(e.io_latency-IFNULL(s.io_latency, 0)) DESC',
     lambda s, e: (e["ios"] - (s["ios"] or 0)) > 0, None, 'user'),
    ('user_summary_by_file_io_type', '%{TABLE}.user, %{TABLE}.latency DESC',
     'e.user, (e.latency-IFNULL(s.latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'user,event_name'),
    ('user_summary_by_stages', '%{TABLE}.user, %{TABLE}.total_latency DESC',
     'e.user, (e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'user,event_name'),
    ('user_summary_by_statement_latency', '%{TABLE}.total_latency DESC',
     '(e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'user'),
    ('user_summary_by_statement_type', '%{TABLE}.user, %{TABLE}.total_latency DESC',
     'e.user, (e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'user,statement'),
    ('wait_classes_global_by_avg_latency', 'IFNULL(%{TABLE}.total_latency / NULLIF(%{TABLE}.total, 0), 0) DESC',
     'IFNULL((e.total_latency-IFNULL(s.total_latency, 0)) / NULLIF((e.total - IFNULL(s.total, 0)), 0), 0) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'event_class'),
    ('wait_classes_global_by_latency', '%{TABLE}.total_latency DESC',
     '(e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'event_class'),
    ('waits_by_host_by_latency', '%{TABLE}.host, %{TABLE}.total_latency DESC',
     'e.host, (e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'host,event'),
    ('waits_by_user_by_latency', '%{TABLE}.user, %{TABLE}.total_latency DESC',
     'e.user, (e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'user,event'),
    ('waits_global_by_latency', '%{TABLE}.total_latency DESC',
     '(e.total_latency-IFNULL(s.total_latency, 0)) DESC',
     lambda s, e: (e["total"] - (s["total"] or 0)) > 0, None, 'events')
]


class SQLExecutor:

    def __init__(self,
                 session: InstanceSession,
                 custom_sql: List[str] = [],
                 allow_phases: bool = True):
        self.session = session
        phases = split_phases(custom_sql)
        if not allow_phases:
            if phases[1] or phases[2]:
                raise Error(
                    "Option 'customSql' may not contain before:, during: or after: prefixes"
                )

        self.custom_sql = []
        for l in phases:
            self.custom_sql.append([(f"script_{i}", sql)
                                    for i, sql in enumerate(l)])

    def execute(self, zf: zipfile.ZipFile, prefix: str):
        l = self.custom_sql[0]
        if l:
            print(f"Executing custom sql scripts")
            collect_queries(zf,
                            f"{prefix}custom_sql-",
                            self.session,
                            l,
                            ignore_errors=False)

    def execute_before(self, zf: zipfile.ZipFile, prefix: str):
        l = self.custom_sql[0]
        if l:
            print(f"Executing custom 'before' sql scripts")
            collect_queries(zf,
                            f"{prefix}custom_sql-before-",
                            self.session,
                            l,
                            ignore_errors=False)

    def execute_during(self, zf: zipfile.ZipFile, prefix: str, iteration: int):
        l = self.custom_sql[1]
        if l:
            print(f"Executing custom 'during' iteration sql scripts")
            collect_queries(zf,
                            f"{prefix}custom_sql-iteration{iteration}-",
                            self.session,
                            l,
                            ignore_errors=False)

    def execute_after(self, zf: zipfile.ZipFile, prefix: str):
        l = self.custom_sql[2]
        if l:
            print(f"Executing custom 'after' sql scripts")
            collect_queries(zf,
                            f"{prefix}custom_sql-after-",
                            self.session,
                            l,
                            ignore_errors=False)


class DiagnosticsSession:

    def __init__(self, session: InstanceSession, innodb_mutex: bool):
        self.session = session
        self.pfs_instrumentation = None
        self.metrics = {}
        self.metrics_info = {}
        self.status_start = {}
        self.status_end = {}

        self.pfs_instrumentation_changed = False
        self.has_performance_analyzer = False

        self.innodb_mutex = innodb_mutex

        if self.session.has_pfs:
            self.pfs_events_wait_history_long = self.session.run_sql(
                "select sys.ps_is_consumer_enabled('events_waits_history_long') = 'YES'"
            ).fetch_one()[0]
            self.pfs_memory_instrumented = self.session.run_sql(
                "select EXISTS(SELECT 1 FROM performance_schema.setup_instruments WHERE NAME LIKE 'memory/%' AND ENABLED = 'YES')"
            ).fetch_one()[0]
        else:
            self.pfs_events_wait_history_long = False
            self.pfs_memory_instrumented = False

        if self.session.has_ndb:
            self.ndbinfo_tables = [
                "ndbinfo." + t[0] for t in self.session.run_sql(
                    "show tables in ndbinfo").fetch_all()
            ]
        else:
            self.ndbinfo_tables = []

        self.session.run_sql("set session group_concat_max_len=2048")

        # disable binlog if it's enabled and not RBR, so that temp tables created
        # by sys SPs don't get replicated
        log_bin, binlog_format = self.session.run_sql(
            "select @@sql_log_bin, @@binlog_format").fetch_one()
        if log_bin and binlog_format != "ROW" and self.session.has_pfs:
            self.disabled_binlog = True
            self.session.run_sql("SET SESSION sql_log_bin = 0")
        else:
            self.disabled_binlog = False

    def start(self, zf: zipfile.ZipFile, prefix: str,
              pfs_instrumentation: str):
        assert pfs_instrumentation in ("current", "medium", "full")

        if self.session.has_pfs:
            self.enable_pfs_instruments(pfs_instrumentation)

            self.collect_pfs_config(
                zf, f"{prefix}-applied_{pfs_instrumentation}-")
        else:
            if pfs_instrumentation != "current":
                raise Error(
                    "performance_schema is disabled, instrumentation cannot be changed"
                )

        self.status_start = self.fetch_status("start", zf, prefix)

        self.collect_metrics(zf, prefix, 0)

        if self.session.has_pfs:
            print(" - Preparing statement performance analyzer...")
            try:
                for q in [
                        "DROP TEMPORARY TABLE IF EXISTS sys.tmp_digests_start",
                        "CALL sys.statement_performance_analyzer('create_tmp', 'tmp_digests_start', NULL)",
                        "CALL sys.statement_performance_analyzer('snapshot', NULL, NULL)",
                        "CALL sys.statement_performance_analyzer('save', 'tmp_digests_start', NULL)"
                ]:
                    self.session.run_sql(q)
                self.has_performance_analyzer = True
            except Exception as e:
                print(f"Error executing {q}: {e}")
                print("Related checks will be skipped")
                self.has_performance_analyzer = False

    def iterate(self, zf: zipfile.ZipFile, prefix: str, iteration: int):
        self.collect_metrics(zf, prefix, iteration)
        self.collect_other_stats(zf, prefix, iteration)

    def start_background(self, delay: int, zf: zipfile.ZipFile, prefix: str,
                         iter_fn: Callable):

        def runloop(cond):
            cond.acquire()
            self.end_thread = False
            cond.notify()
            cond.release()
            i = 0
            while not self.end_thread:
                print("Collecting metrics...")
                self.iterate(zf, prefix, i + 1)

                iter_fn(i)

                i += 1
                # use SQL sleep because it can be interrupted
                for _ in range(delay):
                    self.session.run_sql("select sleep(1)")
                    if self.end_thread:
                        break

        self.end_thread = None
        cond = Condition()

        self.thread = Thread(target=runloop, args=(cond, ))
        self.thread.start()

        while self.end_thread is None:
            cond.acquire()
            cond.wait()
            cond.release()

    def stop_background(self):
        self.end_thread = True
        self.thread.join()

    def finish(self, zf: zipfile.ZipFile, prefix: str):
        if self.session.has_pfs:
            print(" - Writing delta of collected metrics...")
            self.dump_metrics_delta(zf, prefix)

            self.status_end = self.fetch_status("end", zf, prefix)
            self.dump_status_delta(zf, prefix)

            for q in ["CALL sys.ps_statement_avg_latency_histogram()"]:
                self.session.run_sql(q)

            if self.has_performance_analyzer:
                dump_query(
                    zf,
                    f"{prefix}-statement_performance_analyzer-delta",
                    self.session,
                    "CALL sys.statement_performance_analyzer('delta', 'tmp_digests_start', 'with_runtimes_in_95th_percentile')",
                    as_yaml=False)

                for q in [
                        "DROP TEMPORARY TABLE IF EXISTS sys.tmp_digests_start",
                        "CALL sys.statement_performance_analyzer('cleanup', NULL, NULL)"
                ]:
                    self.session.run_sql(q)

        self.cleanup(zf, prefix)

    def cleanup(self, zf: zipfile.ZipFile, prefix: str):
        if self.session.has_pfs:
            self.restore_pfs_instruments()

        if self.disabled_binlog:
            self.session.run_sql("SET sql_log_bin = 1")

    def enable_pfs_instruments(self, level):
        self.pfs_instrumentation = level

        self.session.run_sql(
            "CALL sys.ps_setup_disable_thread(CONNECTION_ID())")

        if level == "current":
            c = self.session.run_sql(
                "select count(*) from performance_schema.setup_consumers where enabled='YES'"
            ).fetch_one()[0]
            if c == 0:
                print(
                    "WARNING: performance_schema.setup_consumers is completely disabled."
                )

            if self.session.version >= 80000:
                c = self.session.run_sql(
                    "select count(*) from performance_schema.setup_threads where enabled='YES'"
                ).fetch_one()[0]
                if c == 0:
                    print(
                        "WARNING: performance_schema.setup_threads is completely disabled."
                    )
        else:
            print("Configuring Performance Schema Instrumentation")
            self.session.run_sql("CALL sys.ps_setup_save(0)")
            self.pfs_instrumentation_changed = True

            if level == "medium":
                # Enable all consumers except % history and %history_long
                self.session.run_sql(
                    """UPDATE performance_schema.setup_consumers
                    SET ENABLED = 'YES'
                WHERE NAME NOT LIKE '%\\_history%'""")

                # Enable all instruments except wait/synch/%
                self.session.run_sql(
                    """UPDATE performance_schema.setup_instruments
                    SET ENABLED = 'YES',
                        TIMED = 'YES'
                WHERE NAME NOT LIKE 'wait/synch/%'""")
            elif level == "full":
                self.session.run_sql(
                    """UPDATE performance_schema.setup_consumers
                    SET ENABLED = 'YES'""")

                self.session.run_sql(
                    """UPDATE performance_schema.setup_instruments
                    SET ENABLED = 'YES',
                        TIMED = 'YES'""")

            # Enable all threads except this one
            self.session.run_sql("""UPDATE performance_schema.threads
            SET INSTRUMENTED = 'YES'
            WHERE PROCESSLIST_ID <> CONNECTION_ID()""")

    def restore_pfs_instruments(self):
        if self.pfs_instrumentation != "current" and self.pfs_instrumentation_changed:
            print("Restoring Performance Schema Configurations")
            self.session.run_sql("CALL sys.ps_setup_reload_saved()")
            self.pfs_instrumentation_changed = False

    def fetch_status(self, stage: str, zf: zipfile.ZipFile, prefix: str):

        def formatters_for_table(table: str) -> List[Callable]:
            """
            Assemble a query on a sys view, formatting columns to be human readable.
            """
            columns = self.session.run_sql(
                "select column_name from information_schema.columns where table_schema='sys' and table_name=? order by ordinal_position",
                [table]).fetch_all()
            formatters = []
            for column, in columns:
                lcolumn = column.lower()
                if (table == "io_global_by_file_by_bytes" and lcolumn
                        == "total") or (table == "io_global_by_wait_by_bytes"
                                        and lcolumn == "total_requested"):
                    formatters.append(format_bytes)
                elif lcolumn.endswith("latency"):
                    formatters.append(format_pico_time)
                elif (lcolumn.endswith("_memory")
                      or lcolumn.endswith("_memory_allocated") or
                      lcolumn.endswith("_read") or lcolumn.endswith("_written")
                      or lcolumn.endswith("_write")
                      ) and not lcolumn.startswith("count_"):
                    formatters.append(format_bytes)
                else:
                    formatters.append(lambda f: f)
            return formatters

        status = {}
        for table_name, order_by, *_ in k_sys_views_delta:
            assert order_by
            order_by = " ORDER BY " + \
                order_by.replace("%{TABLE}", f"x${table_name}")

            query = f"SELECT * FROM `sys`.`x${table_name}`{order_by or ''}"
            formatters = formatters_for_table(table_name)

            print(f" - Gathering sys.{table_name}...")
            status[table_name] = dump_query(
                zf,
                f"{prefix}-raw/{stage}.{table_name}",
                self.session,
                query,
                as_yaml=False,
                formatters=formatters)

        return status

    def dump_status_delta(self, zf: zipfile.ZipFile, prefix: str):

        def formatters_for_table(table_name: str,
                                 pk: str) -> Tuple[List[str], List[Callable]]:
            columns = [
                r[0] for r in self.session.run_sql(
                    "select column_name from information_schema.columns where table_schema='sys' and table_name=? order by ordinal_position",
                    [table_name]).fetch_all()
            ]
            fcolumns = []

            def float_or_none(s):
                if s is None:
                    return s
                return float(s)

            for column in columns:
                lcolumn = column.lower()

                if "," + lcolumn + "," in "," + pk + ",":
                    fcolumns.append(lambda s, e: f"{s[column]}")
                elif table_name == "io_global_by_file_by_bytes" and lcolumn == "write_pct":

                    def fmt(s, e):
                        e_read = float(e["total_read"] or 0)
                        s_read = float_or_none(s["total_read"])
                        e_written = float(e["total_written"] or 0)
                        s_written = float_or_none(s["total_written"])
                        if s_read is None or s_written is None or (
                                e_read - s_read) + (e_written -
                                                    s_written) == 0:
                            return None

                        return f"{100 - ((e_read-s_read) / ((e_read - s_read) + (e_written - s_written))) * 100:.2f}"

                    fcolumns.append(fmt)
                elif (table_name, lcolumn) in [
                    ("io_global_by_file_by_bytes", "total"),
                    ("io_global_by_wait_by_bytes", "total_requested")
                ]:

                    def fmt(s, e):
                        e = float_or_none(e[column])
                        s = float(s[column] or 0)
                        if e is None:
                            return None
                        return format_bytes(e - s)

                    fcolumns.append(fmt)
                elif lcolumn[:4] in ('max_',
                                     'min_') and lcolumn.endswith('_latency'):
                    fcolumns.append(lambda s, e: (format_pico_time(
                        float(e[column])) if e[column] is not None else None))
                elif lcolumn == 'avg_latency':

                    def fmt(s, e):
                        e_lat = float(e["total_latency"] or 0)
                        s_lat = float_or_none(s["total_latency"])
                        e_total = float(e["total"] or 0)
                        s_total = float_or_none(s["total"])
                        if s_lat is None or s_total is None or e_total - s_total == 0:
                            return None
                        return format_pico_time(
                            (e_lat - s_lat) / (e_total - s_total))

                    fcolumns.append(fmt)
                elif lcolumn.endswith('_avg_latency'):
                    prefix = column[:-12]

                    def fmt(s, e):
                        e_lat = float(e[f"{prefix}_latency"] or 0)
                        s_lat = float_or_none(s[f"{prefix}_latency"])
                        e_total = float(e[f"{prefix}s"] or 0)
                        s_total = float_or_none(s[f"{prefix}s"])
                        if s_lat is None or s_total is None or e_total - s_total == 0:
                            return None
                        return format_pico_time(
                            (e_lat - s_lat) / (e_total - s_total))

                    fcolumns.append(fmt)
                elif column.endswith('latency'):
                    fcolumns.append(lambda s, e: (format_pico_time(
                        (float(e[column]) - float(s[column] or 0))
                        if e[column] is not None else None)))
                elif column in ('avg_read', 'avg_write', 'avg_written'):
                    suffix = "read" if lcolumn == "avg_read" else "written"
                    suffix2 = "read" if lcolumn == "avg_read" else "write"

                    def fmt(s, e):
                        e_total = float(s[f"total_{suffix}"] or 0)
                        s_total = float_or_none(s[f"total_{suffix}"])
                        e_count = float(e[f"count_{suffix2}"] or 0)
                        s_count = float_or_none(s[f"count_{suffix2}"])
                        if s_total == None or s_count is None:
                            return None
                        n = e_total - s_total
                        d = e_count - s_count
                        if d == 0:
                            return None
                        return format_bytes(n / d)

                    fcolumns.append(fmt)
                elif lcolumn.endswith("_memory") or lcolumn.endswith(
                        '_memory_allocated'
                ) or (lcolumn.endswith("_read") or lcolumn.endswith('_written')
                      or lcolumn.endswith("_write")
                      ) and not column.startswith("count_"):

                    def fmt(s, e):
                        e_ = float_or_none(e[column])
                        s_ = float(s[column] or 0)

                        if e_ is None:
                            return None

                        return format_bytes(e_ - s_)

                    fcolumns.append(fmt)
                else:

                    def fmt(s, e):
                        s_ = float(s[column] or 0)
                        e_ = float_or_none(e[column])
                        if e_ is None:
                            return None
                        return f"{e_ - s_}"

                    fcolumns.append(fmt)

            return columns, fcolumns

        for table_name, *_, pk in k_sys_views_delta:
            columns, formatters = formatters_for_table(table_name, pk)

            lines = []
            lines.append("# " + "\t".join(columns))

            for start_row, end_row in zip(self.status_start[table_name],
                                          self.status_end[table_name]):
                entry = []
                for i, fmt in enumerate(formatters):
                    try:
                        entry.append(fmt(start_row, end_row))
                    except:
                        print(columns[i], start_row, end_row)
                        raise
                lines.append("\t".join([s or "-" for s in entry]))

            write_tsv(zf, f"{prefix}-delta.{table_name}", lines)

    def collect_metrics(self, zf: zipfile.ZipFile, prefix: str,
                        iteration: Optional[int]):
        if not self.session.has_pfs:
            return
        if iteration is None:
            fn = f"{prefix}metrics"
        else:
            fn = f"{prefix}-raw/iteration-{iteration}.metrics"
        metrics = dump_query(
            zf,
            fn,
            self.session,
            "SELECT Variable_name, REPLACE(Variable_value, '\n', '\\\\n') AS Variable_value, Type, Enabled FROM sys.metrics",
            as_yaml=False,
            include_warnings=False)

        for row in metrics:
            name = row["Variable_name"]
            value = row["Variable_value"]
            type = row["Type"]
            enabled = row["Enabled"]
            if "name" not in self.metrics_info:
                self.metrics_info[name] = [type, enabled]
            self.metrics.setdefault(name, []).append(value)

    def collect_configs_and_state(self, zf: zipfile.ZipFile, prefix: str):
        other_pfs_tables = ["host_cache", "persisted_variables"]
        is_tables = ["plugins"]
        mysql_tables = ["audit_log_user", "audit_log_filter"]

        tables = [
            f"performance_schema.{t}" for t in self.session.pfs_tables
            if t.startswith("replication_") or t in other_pfs_tables
        ]
        tables += [
            f"information_schema.{t}" for t in self.session.is_tables
            if t in is_tables
        ]
        tables += [
            f"mysql.{t}" for t in self.session.mysql_tables
            if t in mysql_tables
        ]

        if self.session.has_ndb:
            tables.append("ndbinfo.threadblocks")

        if self.session.has_rapid:
            for t in ["rpd_nodes", "rpd_exec_stats", "rpd_query_stats"]:
                if t in self.session.pfs_tables:
                    tables.append("performance_schema." + t)

        def filter_slave_master_info(row):
            row = list(row)
            row[5] = "*****"
            return row

        if self.session.version >= 80023:
            kw_replica = "REPLICA"
            kw_replicas = "REPLICAS"
        else:
            kw_replica = "SLAVE"
            kw_replicas = "SLAVE HOSTS"

        queries = [
            ("global variables",
             """SELECT g.variable_name name, g.variable_value value /*!80000, i.variable_source source*/
            FROM performance_schema.global_variables g
            /*!80000 JOIN performance_schema.variables_info i ON g.variable_name = i.variable_name */
            ORDER BY name"""),
            "XA RECOVER CONVERT xid",

            # replication configuration
            "SHOW BINARY LOGS",
            f"SHOW {kw_replicas}",
            "SHOW MASTER STATUS",
            f"SHOW {kw_replica} STATUS",
            ("replication master_info",
             """SELECT * FROM mysql.slave_master_info ORDER BY Channel_name""",
             filter_slave_master_info),
            ("replication relay_log_info",
             """SELECT Channel_name, Sql_delay, Number_of_workers, Id
                FROM mysql.slave_relay_log_info ORDER BY Channel_name""")
        ]

        if self.session.has_rapid:
            queries += [
                ("rapid table status",
                 """SELECT rpd_table_id.ID, rpd_table_id.Name, rpd_tables.*
                FROM performance_schema.rpd_table_id, performance_schema.rpd_tables
                WHERE rpd_tables.ID = rpd_table_id.ID
                ORDER BY rpd_table_id.SCHEMA_NAME,rpd_table_id.TABLE_NAME"""),
                ("rapid total table size",
                 "SELECT SUM(IFNULL(SIZE_BYTES,0)) FROM performance_schema.rpd_tables"
                 ),
                ("rapid avail_rnstate nodes",
                 """SELECT IFNULL(SUM(memory_total), 0), IFNULL(SUM(memory_usage), 0), IFNULL(SUM(BASEREL_MEMORY_USAGE),0)
                FROM performance_schema.rpd_nodes
                WHERE status = 'AVAIL_RNSTATE'""")
            ]

        collect_tables(zf, prefix, self.session, tables, as_yaml=True)

        collect_queries(zf, prefix, self.session, queries, as_yaml=True)
        self.collect_pfs_config(zf, prefix)

    def collect_pfs_config(self, zf: zipfile.ZipFile, prefix: str):
        # pfs configuration
        queries = [
            ("pfs actors", "SELECT * FROM performance_schema.setup_actors"),
            ("pfs objects", "SELECT * FROM performance_schema.setup_objects"),
            ("pfs consumers",
             """SELECT NAME AS Consumer, ENABLED, sys.ps_is_consumer_enabled(NAME) AS COLLECTS
            FROM performance_schema.setup_consumers"""),
            ("pfs instruments",
             """SELECT SUBSTRING_INDEX(NAME, '/', 2) AS 'InstrumentClass',
                ROUND(100*SUM(IF(ENABLED = 'YES', 1, 0))/COUNT(*), 2) AS 'EnabledPct',
                ROUND(100*SUM(IF(TIMED = 'YES', 1, 0))/COUNT(*), 2) AS 'TimedPct'
            FROM performance_schema.setup_instruments
            GROUP BY SUBSTRING_INDEX(NAME, '/', 2)
            ORDER BY SUBSTRING_INDEX(NAME, '/', 2)"""),
            ("pfs threads",
             """SELECT `TYPE` AS ThreadType, COUNT(*) AS 'Total', ROUND(100*SUM(IF(INSTRUMENTED = 'YES', 1, 0))/COUNT(*), 2) AS 'InstrumentedPct'
            FROM performance_schema.threads
            GROUP BY TYPE""")
        ]
        collect_queries(zf, prefix, self.session, queries, as_yaml=True)

    def collect_other_stats(self, zf: zipfile.ZipFile, prefix: str,
                            iteration: Optional[int]):
        tables = [
            "performance_schema.metadata_locks", "performance_schema.threads",
            "sys.schema_table_lock_waits", "sys.session_ssl_status",
            "sys.session", "sys.processlist",
            "performance_schema.events_waits_current",
            "information_schema.innodb_trx",
            "information_schema.innodb_metrics"
        ]

        if self.pfs_events_wait_history_long:
            tables.append("sys.latest_file_io")

        if self.pfs_memory_instrumented:
            tables += [
                "sys.memory_by_host_by_current_bytes",
                "sys.memory_by_thread_by_current_bytes",
                "sys.memory_by_user_by_current_bytes",
                "sys.memory_global_by_current_bytes"
            ]

        queries = [
            "SHOW GLOBAL STATUS", "SHOW ENGINE INNODB STATUS",
            "SHOW ENGINE PERFORMANCE_SCHEMA STATUS", "SHOW FULL PROCESSLIST",
            "SHOW OPEN TABLES"
        ]
        if self.innodb_mutex:
            queries += ["SHOW ENGINE INNODB MUTEX"]

        if self.session.has_ndb:
            queries += [
                "SHOW ENGINE NDBCLUSTER STATUS",
                ("ndb memoryusage",
                 """SELECT node_id, memory_type, format_bytes(used) AS used, used_pages, format_bytes(total) AS total, total_pages,
                   ROUND(100*(used/total), 2) AS 'Used %'
            FROM ndbinfo.memoryusage""")
            ]

            tables += [
                t for t in self.ndbinfo_tables if t != "ndbinfo.memoryusage"
            ]
            tables += ["information_schema.FILES"]

        if iteration is not None:
            prefix += f"-raw/iteration-{iteration}."

        collect_tables(zf, prefix, self.session, tables, as_yaml=True)
        collect_queries(zf, prefix, self.session, queries, as_yaml=True)

    def dump_metrics_delta(self, zf: zipfile.ZipFile, prefix: str):
        """
        Format all collected metrics iterations and format into a human readable
        form, also including delta values between each iteration.
        """

        if not self.session.has_pfs:
            return

        # Some metrics variables doesn't make sense in delta and rate calculations even if they are numeric
        # as they really are more like settings or "current" status.
        no_delta_names = [
            'innodb_buffer_pool_pages_total', 'innodb_page_size',
            'last_query_cost', 'last_query_partial_plans',
            'qcache_total_blocks', 'slave_last_heartbeat',
            'ssl_ctx_verify_depth', 'ssl_ctx_verify_mode',
            'ssl_session_cache_size', 'ssl_verify_depth', 'ssl_verify_mode',
            'ssl_version', 'buffer_flush_lsn_avg_rate',
            'buffer_flush_pct_for_dirty', 'buffer_flush_pct_for_lsn',
            'buffer_pool_pages_total', 'lock_row_lock_time_avg',
            'lock_row_lock_time_max', 'innodb_page_size'
        ]

        def asnum(s):
            try:
                return float(s)
            except:
                return None

        # limit column width because some values can be very long with ndb
        max_field_length = 50

        with zf.open(f"{prefix}-metrics.summary.tsv", "w") as f:
            first = True
            for name, values in self.metrics.items():
                if first:
                    line = ["Variable_name"]
                    for i in range(1, len(values) + 1):
                        line.append(f"Output {i}")
                        if i > 1:
                            line.append(f"Delta ({i-1} -> {i})")
                    line += ["Type", "Enabled"]
                    f.write(("\t".join(line) + "\n").encode("utf-8"))
                    first = False

                line = [name]
                no_delta = name in no_delta_names
                prev = None
                prev_time = 0
                nvalue = None
                for i, value in enumerate(values):
                    delta = ""
                    nvalue = asnum(value)

                    line.append(value[:max_field_length])

                    if i > 0 and not no_delta and value is not None:
                        time = float(self.metrics['UNIX_TIMESTAMP()'][i])
                        prev_time = float(self.metrics['UNIX_TIMESTAMP()'][i -
                                                                           1])

                        if nvalue is not None and prev is not None:
                            if nvalue == prev:
                                delta = "0 (0/sec)"
                            else:
                                delta = f"{nvalue - prev} ({(nvalue-prev) / (time-prev_time):.2f}/sec)"
                            line.append(delta)
                    prev = nvalue

                line += self.metrics_info[name]

                f.write(("\t".join(line) + "\n").encode("utf-8"))


def collect_diagnostics(zf: zipfile.ZipFile,
                        prefix: str,
                        session: InstanceSession,
                        iterations: int,
                        delay: float,
                        pfsInstrumentation: str,
                        innodb_mutex: bool = False,
                        custom_sql: List[str] = [],
                        custom_shell: List[str] = []):
    shell = ShellExecutor(custom_shell, allow_phases=True)
    diag = DiagnosticsSession(session, innodb_mutex=innodb_mutex)
    try:
        custom = SQLExecutor(session, custom_sql, allow_phases=True)

        shell.execute_before(zf, prefix)
        custom.execute_before(zf, prefix)

        diag.collect_configs_and_state(zf, f"{prefix}")
        diag.start(zf, f"{prefix}diagnostics", pfsInstrumentation)

        for i in range(iterations):
            print(
                f"Collecting performance metrics (iteration #{i+1} of {iterations})..."
            )
            diag.iterate(zf, f"{prefix}diagnostics", i + 1)

            shell.execute_during(zf, prefix, i)
            custom.execute_during(zf, prefix, i)

            i += 1
            if i <= iterations - 1:
                print(f"Sleeping for {delay}s...")
                # use SQL sleep because it can be interrupted
                try:
                    session.run_sql("select sleep(?)", [delay])
                except KeyboardInterrupt:
                    print("^C - aborting...")
                    raise
            else:
                break
        if iterations > 0:
            print("Performance metrics collection done")

        diag.finish(zf, f"{prefix}diagnostics")

        shell.execute_after(zf, prefix)
        custom.execute_after(zf, prefix)
    except:
        diag.cleanup(zf, prefix)
        raise


def collect_diagnostics_once(zf: zipfile.ZipFile,
                             prefix: str,
                             session: InstanceSession,
                             innodb_mutex: bool = False,
                             custom_sql: List[str] = []):
    diag = DiagnosticsSession(session, innodb_mutex=innodb_mutex)
    try:
        custom = SQLExecutor(session, custom_sql, allow_phases=False)

        custom.execute(zf, prefix)

        diag.collect_configs_and_state(zf, prefix)
        diag.collect_metrics(zf, prefix, None)
        diag.collect_other_stats(zf, prefix, None)
    finally:
        diag.cleanup(zf, prefix)


def collect_cluster_metadata(zf: zipfile.ZipFile, prefix: str,
                             session: InstanceSession, ignore_errors) -> Optional[str]:
    cluster_type = None
    try:
        r = session.run_sql(
            "show full tables in mysql_innodb_cluster_metadata").fetch_all()
    except Error as e:
        if e.code == mysql.ErrorCode.ER_BAD_DB_ERROR or e.code == mysql.ErrorCode.ER_NO_SUCH_TABLE:
            return cluster_type
        if ignore_errors:
            print(f"ERROR: Could not query InnoDB Cluster metadata: {e}")
            with zf.open(
                    make_zipinfo(
                        f"{prefix}mysql_innodb_cluster_metadata.error"),
                    "w") as f:
                f.write(b"show full tables in mysql_innodb_cluster_metadata\n")
                f.write(f"{e}\n".encode("utf-8"))
            return cluster_type
        raise

    rs_possible = False
    cs_possible = False
    print("Dumping mysql_innodb_cluster_metadata schema...")
    for row in r:
        if "async_cluster_views" == row[0]:
            rs_possible = True
        elif "clusterset_views" == row[0]:
            cs_possible = True
        if row[1] != "BASE TABLE" and row[0] not in ("schema_version", ):
            continue
        table = row[0]
        dump_table(zf,
                   f"{prefix}mysql_innodb_cluster_metadata.{table}",
                   session,
                   f"mysql_innodb_cluster_metadata.{table}",
                   as_yaml=True,
                   ignore_errors=ignore_errors)

    cluster_type = "gr"
    if rs_possible or cs_possible:
        t = session.run_sql(
            "select cluster_type from mysql_innodb_cluster_metadata.v2_this_instance").fetch_one()[0]
        if t == "ar":
            cluster_type = "ar"
        else:
            t = session.run_sql(
                "select count(*) from mysql_innodb_cluster_metadata.v2_cs_clustersets").fetch_one()[0]
            if t:
                cluster_type = "cs"
    return cluster_type


def get_topology_members(session: InstanceSession):
    try:
        return [(r[0], r[1]) for r in session.run_sql(
            "select instance_id, endpoint from mysql_innodb_cluster_metadata.v2_instances"
        ).fetch_all()]
    except:
        return [(r[0], r[1]) for r in session.run_sql(
            "select instance_id, addresses->>'$.mysqlClassic' from mysql_innodb_cluster_metadata.instances"
        ).fetch_all()]


def collect_error_log_sql(zf: zipfile.ZipFile, path: str,
                          session: InstanceSession,
                          ignore_errors: bool) -> bool:
    if session.version >= 80022:
        print(" - Gathering error_log")

        def filter_pwd(row):
            if "temporary password" in row[5]:
                return None
            return row

        dump_table(zf,
                   path,
                   session,
                   "performance_schema.error_log",
                   filter=filter_pwd,
                   as_yaml=False,
                   ignore_errors=ignore_errors)

        return True
    else:
        return False


def collect_slow_queries(zf: zipfile.ZipFile,
                         prefix: str,
                         session: InstanceSession,
                         *,
                         ignore_errors: bool = False):
    queries = [
        ("slow queries in 95 pctile",
         "SELECT * FROM sys.statements_with_runtimes_in_95th_percentile"),
        ("slow queries summary by rows examined",
         "SELECT DIGEST, substr(DIGEST_TEXT, 1, 50), COUNT_STAR, SUM_ROWS_EXAMINED, SUM_ROWS_SENT, round(SUM_ROWS_SENT/SUM_ROWS_EXAMINED, 5) ratio FROM performance_schema.events_statements_summary_by_digest where DIGEST_TEXT like 'select%' and (SUM_ROWS_SENT/SUM_ROWS_EXAMINED) < .5 ORDER BY SUM_ROWS_EXAMINED/SUM_ROWS_SENT desc limit 20"
         )
    ]

    if "slow_log" in session.mysql_tables:
        queries.append(("slow_log", "SELECT * FROM mysql.slow_log"))

    collect_queries(zf,
                    prefix,
                    session,
                    queries,
                    as_yaml=True,
                    ignore_errors=ignore_errors)


def collect_innodb_cluster_accounts(zf: zipfile.ZipFile,
                                    prefix: str,
                                    session: InstanceSession,
                                    ignore_errors=False):
    print("Collecting InnoDB Cluster accounts and grant information")
    accounts = session.run_sql(
        "select user,host from mysql.user where user like 'mysql_innodb_%'"
    ).fetch_all()
    with zf.open(make_zipinfo(f"{prefix}cluster_accounts.tsv"), "w") as f:
        for row in accounts:
            user, host = row[0], row[1]
            f.write(f"-- {user}@{host}\n".encode("utf-8"))
            try:
                for r in session.run_sql("show grants for ?@?",
                                         args=[user, host]).fetch_all():
                    f.write((r[0] + "\n").encode("utf-8"))
            except Exception as e:
                if ignore_errors:
                    print(
                        f"WARNING: Error getting grants for {user}@{host}: {e}"
                    )
                    f.write(f"Could not get grants for {user}@{host}: {e}\n".
                            encode("utf-8"))
                else:
                    print(
                        f"ERROR: Error getting grants for {user}@{host}: {e}")
                    raise
            f.write(b"\n")


def collect_schema_stats(zf: zipfile.ZipFile,
                         prefix: str,
                         session: InstanceSession,
                         full=False,
                         ignore_errors=False):
    if full:
        print(f"Collecting Schema Information and Statistics")
    queries = [
        ("schema tables without a PK",
         """SELECT t.table_schema, t.table_name, t.table_rows, t.engine, t.data_length, t.index_length
            FROM information_schema.tables t
              LEFT JOIN information_schema.statistics s on t.table_schema=s.table_schema and t.table_name=s.table_name and s.index_name='PRIMARY'
            WHERE s.index_name is NULL and t.table_type = 'BASE TABLE'
                and t.table_schema not in ('performance_schema', 'sys', 'mysql', 'information_schema')"""
         ),
        ("schema routine size",
         "SELECT ROUTINE_TYPE, COUNT(*), SUM(LENGTH(ROUTINE_DEFINITION)) FROM information_schema.ROUTINES GROUP BY ROUTINE_TYPE;"
         ),
        ("schema table count",
         "SELECT count(*) FROM information_schema.tables"),
        ("schema unused indexes", "SELECT * FROM sys.schema_unused_indexes"),
    ]
    if full:
        queries += [
            ("schema object overview",
             "select * from sys.schema_object_overview"),
            ("schema top biggest tables",
             """select t.table_schema, t.table_name, t.row_format, t.table_rows, t.avg_row_length, t.data_length, t.max_data_length, t.index_length, t.table_collation,
        json_objectagg(idx.index_name, json_object('columns', idx.col, 'type', idx.index_type, 'cardinality', idx.cardinality)) indexes,
        group_concat((select concat(c.column_name, ':', c.column_type)
          from information_schema.columns c
          where c.table_schema = t.table_schema and c.table_name = t.table_name and c.column_type in ('blob'))) blobs
    from information_schema.tables t
    join (select s.table_schema, s.table_name, s.index_name, s.index_type, s.cardinality, json_arrayagg(concat(c.column_name, ':', c.column_type)) col
          from information_schema.statistics s left join information_schema.columns c on s.table_schema=c.table_schema and s.table_name=c.table_name and s.column_name=c.column_name
          group by s.table_schema, s.table_name, s.index_name, s.index_type, s.cardinality
          order by s.table_schema, s.table_name, s.index_name, s.index_type, s.cardinality) idx
    on idx.table_schema=t.table_schema and idx.table_name = t.table_name
    where t.table_type = 'BASE TABLE' and t.table_schema not in ('mysql', 'information_schema', 'performance_schema')
    group by t.table_schema, t.table_name, t.engine, t.row_format, t.table_rows, t.avg_row_length, t.data_length, t.max_data_length, t.index_length, t.table_collation
    order by t.data_length desc limit 20"""),
            ("schema table engines", """SELECT ENGINE, COUNT(*) AS NUM_TABLES,
                sys.format_bytes(SUM(DATA_LENGTH)) AS DATA_LENGTH,
                sys.format_bytes(SUM(INDEX_LENGTH)) AS INDEX_LENGTH,
                sys.format_bytes(SUM(DATA_LENGTH+INDEX_LENGTH)) AS TOTAL
            FROM information_schema.TABLES
            GROUP BY ENGINE"""),
            ("schema table info", "SELECT * FROM information_schema.TABLES")
        ]
    collect_queries(zf,
                    f"{prefix}",
                    session,
                    queries,
                    as_yaml=True,
                    ignore_errors=ignore_errors)


def collect_table_info(zf: zipfile.ZipFile, prefix: str,
                       session: InstanceSession, schema: str, table: str):
    with zf.open(
            make_zipinfo(
                f"{prefix}referenced_table-{sanitize(schema)}.{sanitize(table)}.yaml"
            ), "w") as f:

        def pythonize(d):
            return json.loads(str(d).replace("\n", "\\n"))

        table_status = session.run_sql(
            "SELECT * FROM information_schema.tables WHERE table_schema=? AND table_name=?",
            [schema, table]).fetch_one_object()

        if not table_status:
            print(
                f"WARNING: Could not find table `{schema}`.`{table}` referenced in query (try 'USE schema' first)"
            )
            return

        table_status = pythonize(table_status)

        indexes = []
        triggers = []
        table_stats = []
        index_stats = []
        if table_status["TABLE_TYPE"] == "VIEW":
            ddl = session.run_sql(
                f"SHOW CREATE VIEW {mysql.quote_identifier(schema)}.{mysql.quote_identifier(table)}"
            ).fetch_one()[0]
        else:
            ddl = session.run_sql(
                f"SHOW CREATE TABLE {mysql.quote_identifier(schema)}.{mysql.quote_identifier(table)}"
            ).fetch_one()[1]

            res = session.run_sql(
                f"SHOW INDEX IN {mysql.quote_identifier(schema)}.{mysql.quote_identifier(table)}"
            )
            for r in iter(res.fetch_one_object, None):
                indexes.append(pythonize(r))

            res = session.run_sql(
                "SELECT * FROM information_schema.triggers WHERE EVENT_OBJECT_SCHEMA=? AND EVENT_OBJECT_TABLE=?",
                [schema, table])
            for r in iter(res.fetch_one_object, None):
                triggers.append(pythonize(r))

            res = session.run_sql(
                "SELECT * FROM mysql.innodb_table_stats WHERE database_name=? AND table_name=?",
                [schema, table])
            for r in iter(res.fetch_one_object, None):
                table_stats.append(pythonize(r))

            res = session.run_sql(
                "SELECT * FROM mysql.innodb_index_stats WHERE database_name=? AND table_name=?",
                [schema, table])
            for r in iter(res.fetch_one_object, None):
                table_stats.append(pythonize(r))

        info = {
            "Table Name": table,
            "Table Schema": schema,
            "Table Status": table_status,
            "DDL": ddl,
            "Indexes": indexes,
            "Triggers": triggers,
            "InnoDB Table Stats": table_stats,
            "InnoDB Index Stats": index_stats
        }

        f.write(yaml.dump(info).encode("utf-8"))


def explain_query(zf: zipfile.ZipFile, session: InstanceSession, query: str,
                  prefix: str) -> dict:
    before = [
        "SET SESSION optimizer_trace='enabled=on'",
        "SET optimizer_trace_offset=-1", "SET optimizer_trace_limit=1"
    ]
    for q in before:
        session.run_sql(q)

    dump_query(zf,
               f"{prefix}explain",
               session,
               f"EXPLAIN {query}",
               as_yaml=False,
               include_warnings=True)
    dump_query(zf,
               f"{prefix}explain-optimizer_trace",
               session,
               "SELECT * FROM information_schema.optimizer_trace",
               as_yaml=True)

    after = ["SET SESSION optimizer_trace='enabled=off'"]
    for q in after:
        session.run_sql(q)

    queries = [(f"explain_json", f"EXPLAIN format=json {query}")]
    if session.version >= 80018:
        queries.append(("explain_analyze", f"EXPLAIN ANALYZE {query}"))
    return collect_queries(zf, prefix, session, queries, include_warnings=True)


def explain_heatwave_query(zf: zipfile.ZipFile, session: InstanceSession,
                           query: str, prefix: str):
    queries = [
        "SET SESSION use_secondary_engine=ON",
        "SELECT NOW()",
        f"EXPLAIN {query}",
        "SHOW SESSION STATUS LIKE 'rapid%'",
        "SELECT NOW()",
        "SET SESSION optimizer_trace='enabled=on'",
        # why -2? idk, see https://dev.mysql.com/doc/heatwave/en/heatwave-running-queries.html#heatwave-debugging-queries
        "SET optimizer_trace_offset=-2",
        f"EXPLAIN {query}",
        "SELECT query, trace->'$**.Rapid_Offload_Fails', trace->'$**.secondary_engine_not_used' FROM information_schema.optimizer_trace",
        "SELECT * FROM information_schema.optimizer_trace",
        "SET SESSION use_secondary_engine=OFF",
        "SET SESSION optimizer_trace='enabled=off'"
    ]

    collect_queries_single_file(zf, f"{prefix}explain-rapid.txt", session,
                                queries)
