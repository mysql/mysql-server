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

from re import X
from typing import List, Tuple, Optional
from mysqlsh import globals
from mysqlsh import mysql, Error
import socket
import zipfile
import yaml
import os
import time
import sys

from debug.host_info import collect_ping_stats, collect_host_info
from debug.sql_collector import make_zipinfo, InstanceSession, collect_diagnostics, collect_diagnostics_once, DiagnosticsSession
from debug.sql_collector import collect_innodb_cluster_accounts, collect_cluster_metadata, collect_error_log_sql, collect_schema_stats, collect_slow_queries, get_topology_members, collect_table_info, explain_heatwave_query, explain_query
from debug.host_info import ShellExecutor
from debug.sql_collector import SQLExecutor


def repr_yaml_text(self, tag, value, style=None):
    if style is None:
        if "\n" in value:
            style = '|'
        else:
            style = self.default_style

    node = yaml.representer.ScalarNode(tag, value, style=style)
    if self.alias_key is not None:
        self.represented_objects[self.alias_key] = node
    return node


yaml.representer.BaseRepresenter.represent_scalar = repr_yaml_text


def copy_local_file(zf: zipfile.ZipFile, path: str, source_path: str):
    with zf.open(make_zipinfo(path), "w") as f:
        with open(source_path, "rb") as inf:
            while True:
                data = inf.read(1024*1024)
                if not data:
                    break
                f.write(data)


def collect_error_log(zf: zipfile.ZipFile, path: str, *, local_target: bool, session: InstanceSession, ignore_errors: bool):
    if local_target:
        sys_datadir, sys_log_error = session.run_sql(
            "select @@datadir, @@log_error").fetch_one()

        if os.path.isabs(sys_log_error):
            log_path = sys_log_error
        else:
            log_path = os.path.join(sys_datadir, sys_log_error)
        print(f" - Copying MySQL error log file ({log_path})")
        copy_local_file(zf, path, log_path)
    else:
        if not collect_error_log_sql(zf, path, session, ignore_errors=ignore_errors):
            print(
                f"MySQL error logs could not be collected for {session}, please include error.log files if reporting bugs or seeking assistance")
            return False
    return True


def collect_member_info(zf: zipfile.ZipFile, prefix: str, session: InstanceSession, slow_queries: bool,
                        ignore_errors: bool, benchmark: bool = False, local_target: bool = False):

    with zf.open(make_zipinfo(f"{prefix}uri"), "w") as f:
        f.write(f"{session.uri}\n".encode("utf-8"))

    utc_time, local_time, time_zone, sys_tz, tz_offs, hostname, port, report_host, report_port, server_uuid, server_id, version = session.run_sql(
        "select utc_timestamp(), now(), @@time_zone, @@system_time_zone, cast(TIMEDIFF(NOW(), UTC_TIMESTAMP()) as char), @@hostname, @@port, @@report_host, @@report_port, @@server_uuid, @@server_id, concat(@@version_comment, ' ', @@version)").fetch_one()

    try:
        sys_version = session.run_sql(
            "select sys_version from sys.version").fetch_one()[0]
    except:
        sys_version = "N/A"

    bm_time = None
    loop_count = 50000000
    # -- should take less than 20 seconds
    if benchmark:
        print("Executing BENCHMARK()...")
        # speed up tests
        if sys.executable.endswith("mysqlshrec"):
            loop_count = 100
        bm_time = session.run_sql(
            f"SELECT BENCHMARK({loop_count},(1234*5678/37485-1298+8596^2))").get_execution_time()

    with zf.open(make_zipinfo(f"{prefix}instance"), "w") as f:
        f.write(
            f"Hostname: {hostname} (report_host={report_host})\n".encode("utf-8"))
        f.write(f"Port: {port} (report_port={report_port})\n".encode("utf-8"))
        f.write(f"Server UUID: {server_uuid}\n".encode("utf-8"))
        f.write(f"Server ID: {server_id}\n".encode("utf-8"))
        f.write(f"Connection Endpoint: {session.uri}\n".encode("utf-8"))
        f.write(f"Version: {version}\n".encode("utf-8"))
        f.write(f"UTC Time: {utc_time}\n".encode("utf-8"))
        f.write(f"Local Time: {local_time}\n".encode("utf-8"))
        f.write(f"Time Zone: {time_zone}\n".encode("utf-8"))
        f.write(f"System Time Zone: {sys_tz}\n".encode("utf-8"))
        f.write(f"Time Zone Offset: {tz_offs}\n".encode("utf-8"))
        f.write(f"SYS Schema Version: {sys_version}\n".encode("utf-8"))
        f.write(f"PFS Enabled: {session.has_pfs}\n".encode("utf-8"))
        f.write(
            f"Has InnoDB Cluster: {session.has_innodbcluster}\n".encode("utf-8"))
        f.write(f"Has NDB: {session.has_ndb}\n".encode("utf-8"))
        f.write(f"Has Rapid: {session.has_rapid}\n".encode("utf-8"))
        if benchmark:
            f.write(
                f"BENCHMARK({loop_count},(1234*5678/37485-1298+8596^2)): {bm_time}\n".encode("utf-8"))

    collect_error_log(zf, f"{prefix}error_log",
                      local_target=local_target,
                      session=session, ignore_errors=ignore_errors)

    if slow_queries:
        collect_slow_queries(zf, f"{prefix}", session,
                             ignore_errors=ignore_errors)


def normalize(data):
    t = getattr(type(data), "__name__", None)
    if t == "Dict":
        data = dict(data)
    elif t == "List":
        data = list(data)
    elif t == "Object":
        shclass = getattr(data, "__mysqlsh_classname__", None)
        if shclass == "Options":
            out = {}
            for k in dir(data):
                if not callable(data[k]):
                    out[k] = normalize(data[k])
            return out

    if isinstance(data, dict):
        out = {}
        for k, v in data.items():
            out[k] = normalize(v)
        return out
    elif isinstance(data, list):
        out = []
        for v in data:
            out.append(normalize(v))
        return out
    else:
        return data


def collect_basic_info(zf: zipfile.ZipFile, prefix: str, session: InstanceSession, info: dict = {}, shell_logs: bool = True):
    shell = globals.shell

    shell_info = dict(info)
    shell_info["version"] = shell.version
    shell_info["options"] = normalize(shell.options)
    shell_info["destination"] = session.uri
    shell_info["hostname"] = socket.gethostname()

    with zf.open(make_zipinfo(f"{prefix}shell_info.yaml"), "w") as f:
        f.write(yaml.dump(shell_info).encode("utf-8"))

    if shell_logs:
        print("Copying shell log file...")
        if shell.options["logFile"] and os.path.exists(shell.options["logFile"]):
            copy_local_file(zf, f"{prefix}mysqlsh.log",
                            shell.options["logFile"])


def collect_member(zf: zipfile.ZipFile, prefix: str,
                   session: InstanceSession,
                   innodb_mutex: bool = False,
                   slow_queries: bool = False,
                   ignore_errors: bool = False,
                   local_target: bool = False,
                   custom_sql: List[str] = []):
    vars = []
    for r in iter(session.run_sql("show global variables").fetch_one_object, None):
        vars.append(r)

    collect_member_info(
        zf, prefix, session, slow_queries=slow_queries,
        ignore_errors=ignore_errors,
        local_target=local_target)

    collect_diagnostics_once(zf, prefix, session,
                             innodb_mutex=innodb_mutex,
                             custom_sql=custom_sql)
    return vars


def collect_topology_diagnostics(zf: zipfile.ZipFile, prefix: str, session: InstanceSession, creds,
                                 innodb_mutex: bool = False,
                                 slow_queries: bool = False,
                                 ignore_errors: bool = False,
                                 local_instance_id: int = -1,
                                 custom_sql: List[str] = []):
    instance_data = []
    members = get_topology_members(session)
    for instance_id, endpoint in members:
        data = {
            "instance_id": instance_id,
            "endpoint": endpoint
        }
        print(f"Collecting information from {endpoint}...")
        try:
            dest = dict(creds)
            dest["host"], dest["port"] = endpoint.split(":")

            session = InstanceSession(mysql.get_session(dest))
        except Error as e:
            print(f"Could not connect to {endpoint}: {e}")
            with zf.open(make_zipinfo(f"{prefix}{instance_id}.connect_error.txt"), "w") as f:
                f.write(f"# {endpoint}\n".encode("utf-8"))
                f.write((str(e)+"\n").encode("utf-8"))
            if e.code == mysql.ErrorCode.ER_ACCESS_DENIED_ERROR:
                raise
            continue

        data["global_variables"] = collect_member(
            zf, f"{prefix}{instance_id}.", session,
            innodb_mutex=innodb_mutex, slow_queries=slow_queries,
            ignore_errors=ignore_errors,
            local_target=instance_id == local_instance_id,
            custom_sql=custom_sql)
        print()

        instance_data.append(data)

    return instance_data


def dump_cluster_status(zf, prefix: str, cluster_type: Optional[str]):

    if cluster_type in (None, "gr", "cs"):
        try:
            status = globals.dba.get_cluster().status({"extended": 2})
            with zf.open(make_zipinfo(f"{prefix}cluster_status.yaml"), "w") as f:
                f.write(yaml.dump(eval(repr(status))).encode("utf-8"))
        except Exception as e:
            with zf.open(make_zipinfo(f"{prefix}cluster_status.error"), "w") as f:
                f.write(b"cluster.status({'extended':2})\n")
                f.write(f"{e}\n".encode("utf-8"))
    if cluster_type in (None, "cs"):
        try:
            status = globals.dba.get_cluster_set().status({"extended": 2})
            with zf.open(make_zipinfo(f"{prefix}cluster_set_status.yaml"), "w") as f:
                f.write(yaml.dump(eval(repr(status))).encode("utf-8"))
        except Exception as e:
            with zf.open(make_zipinfo(f"{prefix}cluster_set_status.error"), "w") as f:
                f.write(b"cluster_set.status({'extended':2})\n")
                f.write(f"{e}\n".encode("utf-8"))
    if cluster_type in (None, "ar"):
        try:
            status = globals.dba.get_replica_set().status({"extended": 2})
            with zf.open(make_zipinfo(f"{prefix}replica_set_status.yaml"), "w") as f:
                f.write(yaml.dump(eval(repr(status))).encode("utf-8"))
        except Exception as e:
            with zf.open(make_zipinfo(f"{prefix}replica_set_status.error"), "w") as f:
                f.write(b"replica_set.status({'extended':2})\n")
                f.write(f"{e}\n".encode("utf-8"))


def default_filename():
    return f"mysql-diagnostics-{time.strftime('%Y%m%d-%H%M%S')}.zip"


def diff_multi_tables(topology_info, get_data, get_key, get_value):
    fields = {}
    different_fields = {}
    for i, instance in enumerate(topology_info):
        for row in get_data(instance):
            key = get_key(row)
            value = get_value(row)
            if key not in fields:
                fields[key] = [None] * i + [value]
                if i > 0:
                    different_fields[key] = True
            else:
                if key not in different_fields:
                    if fields[key][-1] != value:
                        different_fields[key] = True
                fields[key].append(value)
    diff = []
    for key in different_fields.keys():
        diff.append((key, fields[key]))

    return diff


def dump_diff(f, key_label, instance_labels, diff, header):
    def get_column_widths(data):
        column_widths = [0] * len(instance_labels)
        for key, values in data:
            for i, v in enumerate(values):
                column_widths[i] = max(column_widths[i], len(v))
        return column_widths

    if header:
        f.write(f"# {header}\n".encode("utf-8"))

    # find widths of each column
    column_widths = [len(key_label)]
    for k, v in diff:
        column_widths[0] = max(column_widths[0], len(k))
    column_widths += get_column_widths(diff)

    line = [key_label.ljust(column_widths[0])]
    for i, label in enumerate(instance_labels):
        line.append(label.ljust(column_widths[1+i]))
    h = (" | ".join(line) + "\n")
    f.write(h.encode("utf-8"))
    f.write(("="*len(h)+"\n").encode("utf-8"))
    for k, v in diff:
        line = [k.ljust(column_widths[0])]
        for i, value in enumerate(v):
            line.append(value.ljust(column_widths[1+i]))
        f.write((" | ".join(line) + "\n").encode("utf-8"))


def analyze_topology_data(zf: zipfile.ZipFile, prefix, topology_info):
    diff = diff_multi_tables(topology_info,
                             lambda i: i["global_variables"],
                             lambda row: row["Variable_name"],
                             lambda row: row["Value"])
    instance_names = []
    for i in topology_info:
        instance_names.append(i["endpoint"])

    with zf.open(make_zipinfo(f"{prefix}diff.global_variables.txt"), "w") as f:
        if diff:
            dump_diff(
                f, "Variable", instance_names, diff, f"{len(diff)} differing global variables between instances")


def process_path(path: str) -> Tuple[str, str]:
    if not path:
        raise Error("'path' cannot be an empty string")

    if not path.lower().endswith(".zip"):
        if path.endswith("/") or (sys.platform == "win32" and path.endswith("\\")):
            path = os.path.join(path, default_filename())
        else:
            path += ".zip"

    prefix = os.path.basename(path.replace("\\", "/"))[:-4] + "/"

    if os.path.exists(path):
        raise Error(path+" already exists")

    return path, prefix


def do_collect_diagnostics(session_, path, orig_args,
                           innodbMutex=False,
                           allMembers=False,
                           schemaStats=False,
                           slowQueries=False,
                           ignoreErrors=False,
                           customSql: List[str] = [],
                           customShell: List[str] = []):
    path, prefix = process_path(path)

    session = InstanceSession(session_)
    if session.version < 50700:
        raise Error("MySQL 5.7 or newer required")

    if slowQueries:
        row = session.run_sql(
            "select @@slow_query_log, @@log_output").fetch_one()
        if not row[0] or row[1] != "TABLE":
            raise Error(
                "slowQueries option requires slow_query_log to be enabled and log_output to be set to TABLE")

    shell = globals.shell
    my_instance_id = session.instance_id

    creds = None
    if allMembers and my_instance_id:
        creds = shell.parse_uri(session.uri)
        creds["password"] = shell.prompt(
            f"Password for {creds['user']}: ", {"type": "password"})

    target = globals.shell.parse_uri(session.uri)
    local_target = True if "host" not in target else target["host"] == "localhost"
    if customShell and not local_target:
        raise Error(
            "Option 'customShell' is only allowed when connected to localhost")

    shell_exe = ShellExecutor(customShell, allow_phases=False)

    print(f"Collecting diagnostics information from {session.uri}...")
    try:
        with zipfile.ZipFile(path, mode="w") as zf:
            os.chmod(path, 0o600)
            if local_target:
                shell_exe.execute(zf, prefix)

            cluster_type = None
            if session.has_innodbcluster:
                print("InnoDB Cluster detected")
                cluster_type = collect_cluster_metadata(
                    zf, prefix, session, ignore_errors=ignoreErrors)
            collect_basic_info(zf, prefix, session,
                               info={"command": "collectDiagnostics",
                                     "commandOptions": normalize(orig_args)})
            collect_schema_stats(zf, prefix, session, full=schemaStats,
                                 ignore_errors=ignoreErrors)

            topology_data = None
            collected = False
            if session.has_innodbcluster and my_instance_id:
                print("Gathering grants for mysql_innodb_% accounts...")
                collect_innodb_cluster_accounts(zf, prefix, session,
                                                ignore_errors=ignoreErrors)

                print("Gathering cluster status...")
                dump_cluster_status(zf, prefix, cluster_type)

                if allMembers and my_instance_id:
                    collected = True
                    topology_data = collect_topology_diagnostics(
                        zf, prefix, session, creds, innodb_mutex=innodbMutex,
                        slow_queries=slowQueries, ignore_errors=ignoreErrors,
                        local_instance_id=my_instance_id if local_target else -1)

                    analyze_topology_data(zf, prefix, topology_data)

            if not collected:
                collect_member(zf, f"{prefix}{my_instance_id}.",
                               session, slow_queries=slowQueries,
                               innodb_mutex=innodbMutex,
                               ignore_errors=ignoreErrors,
                               local_target=local_target,
                               custom_sql=customSql)

            # collect local host info if we're connected to localhost
            target = shell.parse_uri(session.uri)
            if topology_data:
                if local_target:
                    print("Connected to local server, collecting ping stats...")
                    collect_ping_stats(zf, prefix, topology_data)
                else:
                    print(
                        "Connected to remote server, ping stats not be collected.")

            # collect system info
            if local_target:
                collect_host_info(zf, prefix)
    except:
        if os.path.exists(path):
            os.remove(path)
        print()
        print("An error occurred during data collection. Partial output deleted.")
        raise

    print()
    print(f"Diagnostics information was written to {path}")
    if not session.has_pfs:
        print("WARNING: performance_schema is disabled, collected a limited amount of information")
    if allMembers and not session.has_innodbcluster:
        print("NOTE: allMembers enabled, but InnoDB Cluster metadata not found")


def collect_common_high_load_data(zf: zipfile.ZipFile, prefix: str, session: InstanceSession, info: dict, local_target: bool):
    # collect general info
    collect_basic_info(zf, prefix, session,
                       info=info,
                       shell_logs=False)

    collect_schema_stats(zf, prefix, session,
                         full=True, ignore_errors=False)

    collect_member_info(zf, prefix, session,
                        slow_queries=True,
                        ignore_errors=False,
                        local_target=local_target,
                        benchmark=True)

    # collect system info
    if local_target:
        collect_host_info(zf, prefix)


def do_collect_high_load_diagnostics(session_, path, orig_args,
                                     innodbMutex=False,
                                     iterations=2,
                                     delay=5*60,
                                     pfsInstrumentation="current",
                                     customSql: List[str] = [],
                                     customShell: List[str] = []):
    path, prefix = process_path(path)

    if iterations < 1:
        raise Error(f"'iterations' must be > 0 (is {iterations})")
    if delay < 1:
        raise Error(f"'delay' must be > 0 (is {delay})")
    if pfsInstrumentation not in ("current", "medium", "full"):
        raise Error(
            "'pfsInstrumentation' must be one of current, medium, full")

    session = InstanceSession(session_)

    if session.version < 50700:
        raise Error("MySQL 5.7 or newer required")

    target = globals.shell.parse_uri(session.uri)
    local_target = True if "host" not in target else target["host"] == "localhost"
    if customShell and not local_target:
        raise Error(
            "Option 'customShell' is only allowed when connected to localhost")

    print(f"Collecting diagnostics information from {session.uri}...")
    try:
        with zipfile.ZipFile(path, mode="w") as zf:
            os.chmod(path, 0o600)
            collect_common_high_load_data(zf, prefix, session,
                                          {"command": "collectHighLoadDiagnostics",
                                           "commandOptions": normalize(orig_args)},
                                          local_target)

            assert not customShell or local_target

            print()
            collect_diagnostics(zf, prefix, session,
                                iterations, delay, pfsInstrumentation,
                                innodb_mutex=innodbMutex,
                                custom_sql=customSql,
                                custom_shell=customShell)
    except:
        if os.path.exists(path):
            os.remove(path)
        print()
        print("An error occurred during data collection. Partial output deleted.")
        raise

    print()
    if not session.has_pfs:
        print("WARNING: performance_schema is disabled, collected a limited amount of information")
    if not local_target:
        print("NOTE: Target server is not at localhost, host information was not collected")
    if not session.has_pfs:
        print("NOTE: performance_schema is disabled, only a limited set of metrics and information could be collected")
    print(f"Server load diagnostics information was written to {path}")


def extract_referenced_tables(session: InstanceSession, query: str) -> List[Tuple[str, str]]:
    default_schema = session.run_sql("select schema()").fetch_one()[0]

    def find_table_refs(ast):
        def extract_table_ref(node):
            if "rule" in node and node["rule"] == "pureIdentifier":
                if node["children"][0]["symbol"] == "BACK_TICK_QUOTED_ID":
                    return [mysql.unquote_identifier(node["children"][0]["text"])]
                else:
                    return [node["children"][0]["text"]]
            elif "children" in node:
                ref = []
                for n in node["children"]:
                    ref += extract_table_ref(n)
                return ref
            return []

        refs = []
        if "rule" in ast and ast["rule"] == "tableRef":
            table_ref = extract_table_ref(ast)
            assert table_ref
            if len(table_ref) == 1:
                table_ref = default_schema, table_ref[0]
            refs.append(table_ref)
        elif "children" in ast:
            for child in ast["children"]:
                refs += find_table_refs(child)
        return refs

    ast = mysql.parse_statement_ast(query)

    return find_table_refs(ast)


def execute_profiled_query(zf: zipfile.ZipFile, prefix: str, helper_session, query: str):
    print(f"Executing query: {query}")
    r = helper_session.run_sql(query)
    print(f"Query finished in {r.get_execution_time()}")

    fetch_start = time.time()
    row_count = len(r.fetch_all())
    fetch_end = time.time()
    print(f"Results fetched in {fetch_end-fetch_start:.4f} sec")

    query_info = {
        "Execution Time": r.get_execution_time(),
        "Result Fetch Time": (fetch_end - fetch_start),
        "Query": query,
        "Warnings": normalize(r.get_warnings()),
        "Affected Rows": r.get_affected_row_count(),
        "Result Rows": row_count
    }
    if hasattr(r, "get_info"):
        query_info["Info"] = r.get_info()

    return query_info


def do_collect_slow_query_diagnostics(session_, path: str, query: str, orig_args,
                                      innodbMutex=False,
                                      delay=15,
                                      pfsInstrumentation="current",
                                      customSql: List[str] = [],
                                      customShell: List[str] = []):
    if not query:
        raise Error("'query' must contain the query to be analyzed")
    path, prefix = process_path(path)

    if delay < 1:
        raise Error(f"'delay' must be > 0 (is {delay})")
    if pfsInstrumentation not in ("current", "medium", "full"):
        raise Error(
            "'pfsInstrumentation' must be one of current, medium, full")

    session = InstanceSession(session_)
    if session.version < 50700:
        raise Error("MySQL 5.7 or newer required")

    target = globals.shell.parse_uri(session.uri)
    local_target = True if "host" not in target else target["host"] == "localhost"
    if customShell and not local_target:
        raise Error(
            "Option 'customShell' is only allowed when connected to localhost")

    referenced_tables = extract_referenced_tables(session, query)

    helper_session = globals.shell.open_session()

    print(f"Collecting diagnostics information from {session.uri}...")
    try:
        with zipfile.ZipFile(path, mode="w") as zf:
            os.chmod(path, 0o600)
            shell = ShellExecutor(customShell, allow_phases=True)
            custom = SQLExecutor(session, customSql, allow_phases=True)

            shell.execute_before(zf, prefix)
            custom.execute_before(zf, prefix)

            for s, t in referenced_tables:
                print(
                    f"Collecting information for referenced table `{s}`.`{t}`")
                collect_table_info(zf, prefix, session, s, t)

            collect_common_high_load_data(zf, prefix, session,
                                          {"command": "collectSlowQueryDiagnostics",
                                           "commandOptions": normalize(orig_args)},
                                          local_target)

            print("Collecting EXPLAIN")
            explain_query(zf, session, query, prefix)

            if session.has_rapid:
                print("Collecting EXPLAIN with Heatwave")
                explain_heatwave_query(zf, session, query, prefix)

            diag = DiagnosticsSession(session, innodb_mutex=innodbMutex)
            diag.collect_configs_and_state(zf, prefix)
            try:
                diag.start(zf, f"{prefix}diagnostics",
                           pfs_instrumentation=pfsInstrumentation)

                print("Starting background diagnostics collector...")

                def on_iterate(i):
                    shell.execute_during(zf, prefix, i)
                    custom.execute_during(zf, prefix, i)

                diag.start_background(delay, zf, f"{prefix}diagnostics",
                                      on_iterate)

                try:
                    query_info = execute_profiled_query(
                        zf, prefix, helper_session, query)
                except:
                    diag.stop_background()
                    raise
            except:
                diag.cleanup(zf, prefix)
                raise

            diag.stop_background()

            diag.finish(zf, f"{prefix}diagnostics")

            with zf.open(f"{prefix}query-info.yaml", "w") as f:
                f.write(yaml.dump(query_info).encode("utf-8"))
                f.write(b"\n")

            shell.execute_after(zf, prefix)
            custom.execute_after(zf, prefix)
    except:
        helper_session.close()
        if os.path.exists(path):
            os.remove(path)
        print()
        print("An error occurred during query profiling. Partial output deleted.")
        raise

    helper_session.close()

    print()
    if not session.has_pfs:
        print("WARNING: performance_schema is disabled, collected a limited amount of information")
    if not referenced_tables:
        print("NOTE: could not extract list of referenced tables from query")
    if not local_target:
        print("NOTE: Target server is not at localhost, host information was not collected")
    print(f"Server and query diagnostics information was written to {path}")
