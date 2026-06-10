<!---
Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms, as
designated in a particular file or component or in included license
documentation. The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.
//
This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
the GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
-->

# Testing MySQL with OpenTelemetry

This document describes how to perform local tests,
typically in a developer environment.

Many different deployments are possible,
this is just an example.

Adjust paths and setup as needed.

## CMake build

Use `-DWITH_MYSQL_SERVER_TELEMETRY=ON` to compile the server telemetry
component.

Use `-DWITH_MYSQL_CLIENT_TELEMETRY=ON` to compile the telemetry_client
plugin.

## Software dependencies

Podman (alternative to docker).

Opentelemetry-collector:
[github](https://github.com/open-telemetry/opentelemetry-collector)

Jaeger:
[github](https://github.com/jaegertracing/jaeger)

Zipkin:
[github](https://github.com/openzipkin/zipkin)

Prometheus:
[github](https://github.com/prometheus/prometheus)

Loki:
[github](https://github.com/grafana/loki)

Graphana:
[github](https://github.com/grafana/grafana)

## Target deployment (demo)

Processes involved are:

* MySQL Server
* MySQL client(s)
* The OpenTelemetry Collector
* Jaeger
* Zipkin
* Prometheus
* Loki
* Graphana
* A Web browser

Topology is as follows:

* MySQL client(s)
  * send requests to the MySQL server
  * send client telemetry traces to the OpenTelemetry Collector

* MySQL server
  * send server telemetry traces to the OpenTelemetry Collector
  * send server telemetry metrics to the OpenTelemetry Collector
  * send server telemetry logs to the OpenTelemetry Collector

* OpenTelemetry Collector
  * forwards traces to Jaeger
  * forwards traces to Zipkin
  * forwards metrics to Prometheus
  * forwards logs to Loki

* Loki
  * stores logs to minio

* Graphana
  * Reads logs from Loki

* Web browser
  * inspect traces from Jaeger
  * inspect traces from Zipkin
  * inspect metrics from Prometheus
  * inspect logs from Graphana

Connectivity:

* MySQL server
  * exposes the MySQL service, on the usual port
    (possibly allocated by MTR when running tests)

* OpenTelemetry Collector
  * exposes an OTLP HTTP service, on port 4318
  * exposes an OTLP GRPC service, on port 4317 (not used)

* Jaeger
  * exposes an OTLP HTTP service, on port 14318
  * exposes an OTLP GRPC service, on port 14317 (not used)
  * exposes the HTTP UI, on port 16686
  * exposes the HTTP admin UI, on port 14269

* Zipkin
  * exposes an OTLP HTTP service, on port 9411

* Loki
  * exposes an OTLP HTTP service, on port 3100

* Graphana
  * exposes an HTTP service, on port 3000

* Prometheus
  * exposes an HTTP service, on port 9090

## Setup (podman compose demo)

### Prepare the infrastructure

```shell
podman compose build
```

### Start the infrastructure

```shell
podman compose up -d
```

### Stop the infrastructure

```shell
podman compose down
```
### MySQL Server

#### Manual INSTALL COMPONENT

Install the server telemetry component:

```
mysql> INSTALL COMPONENT 'file://component_telemetry';
Query OK, 0 rows affected (0.02 sec)
```

The server telemetry configuration is done with system variables:

```
mysql> show variables like "%telemetry%";
+----------------------------------------------------------+----------------------------------+
| Variable_name                                            | Value                            |
+----------------------------------------------------------+----------------------------------+
| telemetry.metrics_enabled                                | ON                               |
| telemetry.metrics_reader_frequency_1                     | 10                               |
| telemetry.metrics_reader_frequency_2                     | 60                               |
| telemetry.metrics_reader_frequency_3                     | 0                                |
| telemetry.otel_bsp_max_export_batch_size                 | 512                              |
| telemetry.otel_bsp_max_queue_size                        | 2048                             |
| telemetry.otel_bsp_schedule_delay                        | 5000                             |
| telemetry.otel_exporter_otlp_metrics_certificates        |                                  |
| telemetry.otel_exporter_otlp_metrics_cipher              |                                  |
| telemetry.otel_exporter_otlp_metrics_cipher_suite        |                                  |
| telemetry.otel_exporter_otlp_metrics_client_certificates |                                  |
| telemetry.otel_exporter_otlp_metrics_client_key          |                                  |
| telemetry.otel_exporter_otlp_metrics_compression         | none                             |
| telemetry.otel_exporter_otlp_metrics_endpoint            | http://localhost:4318/v1/metrics |
| telemetry.otel_exporter_otlp_metrics_headers             |                                  |
| telemetry.otel_exporter_otlp_metrics_max_tls             |                                  |
| telemetry.otel_exporter_otlp_metrics_min_tls             |                                  |
| telemetry.otel_exporter_otlp_metrics_protocol            | http/protobuf                    |
| telemetry.otel_exporter_otlp_metrics_timeout             | 10000                            |
| telemetry.otel_exporter_otlp_traces_certificates         |                                  |
| telemetry.otel_exporter_otlp_traces_cipher               |                                  |
| telemetry.otel_exporter_otlp_traces_cipher_suite         |                                  |
| telemetry.otel_exporter_otlp_traces_client_certificates  |                                  |
| telemetry.otel_exporter_otlp_traces_client_key           |                                  |
| telemetry.otel_exporter_otlp_traces_compression          | none                             |
| telemetry.otel_exporter_otlp_traces_endpoint             | http://localhost:4318/v1/traces  |
| telemetry.otel_exporter_otlp_traces_headers              |                                  |
| telemetry.otel_exporter_otlp_traces_max_tls              |                                  |
| telemetry.otel_exporter_otlp_traces_min_tls              |                                  |
| telemetry.otel_exporter_otlp_traces_protocol             | http/protobuf                    |
| telemetry.otel_exporter_otlp_traces_timeout              | 10000                            |
| telemetry.otel_log_level                                 | info                             |
| telemetry.otel_resource_attributes                       |                                  |
| telemetry.query_text_enabled                             | ON                               |
| telemetry.trace_enabled                                  | ON                               |
+----------------------------------------------------------+----------------------------------+
35 rows in set (0.02 sec)
```

#### Testing with mysql-test-run.pl

The mysql-test-run.pl script provides an option to preload the telemetry components,
at startup

```
mtr --telemetry
```

To run a test without OpenTelemetry:

```
mtr test_case
```

To run a test with Opentelemetry:

```
mtr --telemetry test_case
```

This is usefull to run a full test suite under telemetry.

### MySQL Client

The client will load the plugin if the `telemetry-client` flag is enabled.

The telemetry_client configuration is located in the my.cnf file,
under a dedicated [telemetry-client] section.

Example of configuration:

```
cat ~/.my.cnf

[mysql]

telemetry-client = ON

[telemetry_client]

otel-help = ON

# opt-trace = OFF

otel-resource-attributes = "RK1=RV1, RK2=RV2, RK3=RV3"

# otel-log-level = "error"

otel-exporter-otlp-traces-endpoint  = "http://localhost:4318/v1/traces"
otel-exporter-otlp-traces-certificates = "path/to/cert"
otel-exporter-otlp-traces-client-key = "path/to/client_key"
otel-exporter-otlp-traces-client-certificates = "path/to/client_cert"
otel-exporter-otlp-traces-headers = "K1=V1, K2=V2"

otel-exporter-otlp-traces-protocol = "http/json"
# otel-exporter-otlp-traces-protocol = "http/protobuf"

```

To start a client with the telemetry_client plugin:

* set `--plugin_dir` to the plugin directory
* load the plugin
  * either set `--telemetry-client` in the command line
  * or set `telemetry-client = ON` in the my.cnf file

For example, when running directly from a build:

```
./runtime_output_directory/mysql \
  --socket=./mysql-test/var/tmp/mysqld.1.sock \
  --user=root \
  --plugin_dir=./plugin_output_directory \
  --telemetry-client
```

Upon startup with the telemetry_client plugin,
the client will display:

```
=== TELEMETRY_CLIENT PLUGIN VARIABLES ===

Variables (--variable-name=value)
and boolean options {FALSE|TRUE}              Value (after reading options)
--------------------------------------------- ----------------------------
otel-help                                     TRUE
otel-trace                                    TRUE
otel-resource-attributes                      RK1=RV1, RK2=RV2, RK3=RV3
otel-log-level                                silent
otel-exporter-otlp-traces-protocol            http/json
otel-exporter-otlp-traces-endpoint            http://localhost:4318/v1/traces
otel-exporter-otlp-traces-certificates        path/to/cert
otel-exporter-otlp-traces-client-key          path/to/client_key
otel-exporter-otlp-traces-client-certificates path/to/client_cert
otel-exporter-otlp-traces-headers             K1=V1, K2=V2
otel-exporter-otlp-traces-compression         none
otel-exporter-otlp-traces-timeout             10000
otel-bsp-schedule-delay                       5000
otel-bsp-max-queue-size                       2048
otel-bsp-max-export-batch-size                512

telemetry_client: Using OTLP HTTP exporter to endpoint <http://localhost:4318/v1/traces>
Telemetry plugin <telemetry_client> is loaded.

Welcome to the MySQL monitor.  Commands end with ; or \g.
Your MySQL connection id is 10
Server version: 8.2.0-debug Source distribution

Copyright (c) 2000, 2024, Oracle and/or its affiliates.

Oracle is a registered trademark of Oracle Corporation and/or its
affiliates. Other names may be trademarks of their respective
owners.

Type 'help;' or '\h' for help. Type '\c' to clear the current input statement.

mysql>

```

The configuration parameters are displayed because `help` is enabled.

### Web browser

Open a browser on file test/demo.html, and follow the links.

## SSL Setup

To use SSL/TLS from MySQL to the opentelemetry collector process,
additional configuration is required.

### Certificates

Generating certificates, or obtaining existing ones,
is outside the scope of this doc.

For example, let's assume certificates are available as follows.

```
malff@dhcp-10-175-36-245.vpn.oracle.com:CERT-LAB> pwd
/home/malff/CERT-LAB
malff@dhcp-10-175-36-245.vpn.oracle.com:CERT-LAB> ls -l *.pem
-rw------- 1 malff users 1675 Dec 13  2022 ca-key.pem
-rw-r--r-- 1 malff users 1155 Dec 13  2022 ca.pem
-rw------- 1 malff users 1675 Dec 13  2022 client_cert-key.pem
-rw-r--r-- 1 malff users 1261 Dec 13  2022 client_cert.pem
-rw------- 1 malff users 1679 Dec 13  2022 server_cert-key.pem
-rw-r--r-- 1 malff users 1261 Dec 13  2022 server_cert.pem

```

### opentelemetry collector configuration

In the opentelemetry collector configuration file,
the section 'tls' for the otlp http receiver describes the SSL/TLS
configuration to use.

For example:

```
receivers:
  otlp:
    protocols:
      # grpc:
      http:
        tls:
          ca_file: /home/malff/CERT-LAB/ca.pem
          cert_file: /home/malff/CERT-LAB/server_cert.pem
          key_file: /home/malff/CERT-LAB/server_cert-key.pem
          min_version: "1.0"
          max_version: "1.3"
```

The full documentation is available at
https://github.com/open-telemetry/opentelemetry-collector/blob/main/config/configtls/README.md



### Telemetry component (server) configuration

When loading the telemetry component,
configure SSL/TLS options as follows.

```
INSTALL COMPONENT 'file://component_telemetry'
  SET
    otel_exporter_otlp_traces_certificates = `/home/malff/CERT-LAB/ca.pem`,
    otel_exporter_otlp_traces_client_certificates = `/home/malff/CERT-LAB/client_cert.pem`,
    otel_exporter_otlp_traces_client_key = `/home/malff/CERT-LAB/client_cert-key.pem`,
    otel_exporter_otlp_traces_max_tls = `1.3`,
    otel_exporter_otlp_traces_min_tls = `1.2`,
    otel_exporter_otlp_metrics_certificates = `/home/malff/CERT-LAB/ca.pem`,
    otel_exporter_otlp_metrics_client_certificates = `/home/malff/CERT-LAB/client_cert.pem`,
    otel_exporter_otlp_metrics_client_key = `/home/malff/CERT-LAB/client_cert-key.pem`,
    otel_exporter_otlp_metrics_max_tls = `1.3`,
    otel_exporter_otlp_metrics_min_tls = `1.2`,
    otel_exporter_otlp_traces_endpoint = `https://localhost:4318/v1/traces`,
    otel_exporter_otlp_metrics_endpoint = `https://localhost:4318/v1/metrics`
;
```

Note that options to the component can be configured in the my.cnf file as
well.

Resulting configuration:

```
mysql> SHOW VARIABLES like "%telemetry%";
+----------------------------------------------------------+------------------------------------------+
| Variable_name                                            | Value                                    |
+----------------------------------------------------------+------------------------------------------+
| telemetry.metrics_enabled                                | ON                                       |
| telemetry.metrics_reader_frequency_1                     | 10                                       |
| telemetry.metrics_reader_frequency_2                     | 60                                       |
| telemetry.metrics_reader_frequency_3                     | 0                                        |
| telemetry.otel_bsp_max_export_batch_size                 | 512                                      |
| telemetry.otel_bsp_max_queue_size                        | 2048                                     |
| telemetry.otel_bsp_schedule_delay                        | 5000                                     |
| telemetry.otel_exporter_otlp_metrics_certificates        | /home/malff/CERT-LAB/ca.pem              |
| telemetry.otel_exporter_otlp_metrics_cipher              |                                          |
| telemetry.otel_exporter_otlp_metrics_cipher_suite        |                                          |
| telemetry.otel_exporter_otlp_metrics_client_certificates | /home/malff/CERT-LAB/client_cert.pem     |
| telemetry.otel_exporter_otlp_metrics_client_key          | /home/malff/CERT-LAB/client_cert-key.pem |
| telemetry.otel_exporter_otlp_metrics_compression         | none                                     |
| telemetry.otel_exporter_otlp_metrics_endpoint            | https://localhost:4318/v1/metrics        |
| telemetry.otel_exporter_otlp_metrics_headers             |                                          |
| telemetry.otel_exporter_otlp_metrics_max_tls             | 1.3                                      |
| telemetry.otel_exporter_otlp_metrics_min_tls             | 1.2                                      |
| telemetry.otel_exporter_otlp_metrics_protocol            | http/protobuf                            |
| telemetry.otel_exporter_otlp_metrics_timeout             | 10000                                    |
| telemetry.otel_exporter_otlp_traces_certificates         | /home/malff/CERT-LAB/ca.pem              |
| telemetry.otel_exporter_otlp_traces_cipher               |                                          |
| telemetry.otel_exporter_otlp_traces_cipher_suite         |                                          |
| telemetry.otel_exporter_otlp_traces_client_certificates  | /home/malff/CERT-LAB/client_cert.pem     |
| telemetry.otel_exporter_otlp_traces_client_key           | /home/malff/CERT-LAB/client_cert-key.pem |
| telemetry.otel_exporter_otlp_traces_compression          | none                                     |
| telemetry.otel_exporter_otlp_traces_endpoint             | https://localhost:4318/v1/traces         |
| telemetry.otel_exporter_otlp_traces_headers              |                                          |
| telemetry.otel_exporter_otlp_traces_max_tls              | 1.3                                      |
| telemetry.otel_exporter_otlp_traces_min_tls              | 1.2                                      |
| telemetry.otel_exporter_otlp_traces_protocol             | http/protobuf                            |
| telemetry.otel_exporter_otlp_traces_timeout              | 10000                                    |
| telemetry.otel_log_level                                 | info                                     |
| telemetry.otel_resource_attributes                       |                                          |
| telemetry.query_text_enabled                             | ON                                       |
| telemetry.trace_enabled                                  | ON                                       |
+----------------------------------------------------------+------------------------------------------+
35 rows in set (0.01 sec)
```

Important considerations:

* From the component point of view, traces, metrics and logs are independent,
  and may or may not be sent to the same endpoint.
* As a result, when all are sent to the same
  collector, the SSL/TLS configuration must be repeated for traces, metrics and
  logs.
* To use a secure connection to the collector, the endpoint MUST use `https`
  instead of `http`.

### Telemetry plugin (client) configuration

When loading the telemetry plugin,
configure SSL/TLS options as follows.

For example, in the my.cnf file:

```
[telemetry_client]

otel-help = ON

otel-exporter-otlp-traces-certificates = "/home/malff/CERT-LAB/ca.pem"
otel-exporter-otlp-traces-client-certificates = "/home/malff/CERT-LAB/client_cert.pem"
otel-exporter-otlp-traces-client-key = "/home/malff/CERT-LAB/client_cert-key.pem"
otel-exporter-otlp-traces-max-tls = "1.3"
otel-exporter-otlp-traces-min-tls = "1.2"

otel-exporter-otlp-traces-endpoint = "https://localhost:4318/v1/traces"
```

Resulting configuration:

```
=== TELEMETRY_CLIENT PLUGIN VARIABLES ===

Variables (--variable-name=value)
and boolean options {FALSE|TRUE}              Value (after reading options)
--------------------------------------------- ----------------------------
otel-help                                     TRUE
otel-trace                                    TRUE
otel-resource-attributes                      RK1=RV1, RK2=RV2, RK3=RV3
otel-log-level                                silent
otel-exporter-otlp-traces-protocol            http/json
otel-exporter-otlp-traces-endpoint            https://localhost:4318/v1/traces
otel-exporter-otlp-traces-certificates        /home/malff/CERT-LAB/ca.pem
otel-exporter-otlp-traces-client-key          /home/malff/CERT-LAB/client_cert-key.pem
otel-exporter-otlp-traces-client-certificates /home/malff/CERT-LAB/client_cert.pem
otel-exporter-otlp-traces-min-tls             1.2
otel-exporter-otlp-traces-max-tls             1.3
otel-exporter-otlp-traces-cipher              (No default value)
otel-exporter-otlp-traces-cipher-suite        (No default value)
otel-exporter-otlp-traces-headers             K1=V1, K2=V2
otel-exporter-otlp-traces-compression         none
otel-exporter-otlp-traces-timeout             10000
otel-bsp-schedule-delay                       5000
otel-bsp-max-queue-size                       2048
otel-bsp-max-export-batch-size                512

telemetry_client: Using OTLP HTTP exporter to endpoint <https://localhost:4318/v1/traces>
Telemetry plugin <telemetry_client> is loaded.

Welcome to the MySQL monitor.  Commands end with ; or \g.
Your MySQL connection id is 11
Server version: 8.3.0-debug Source distribution

Copyright (c) 2000, 2024, Oracle and/or its affiliates.

Oracle is a registered trademark of Oracle Corporation and/or its
affiliates. Other names may be trademarks of their respective
owners.

Type 'help;' or '\h' for help. Type '\c' to clear the current input statement.

mysql>
```

To use a secure connection, the endpoint MUST use `https` instead of `http`.

## Named network setup

To use names networks, additional configuration is required.

This feature is only available on Linux.

### Setup named networks

A sample setup is described here, using two networks, named 'red' and
'blue'.

Start a root shell

```shell
sudo bash
```

In the root shell, define the 'red' network.

```shell
ip netns add red

ip link add veth0 type veth peer name vpeer0

ip link set veth0 up

ip addr add 10.0.2.0/31 dev veth0

ip link set vpeer0 netns red

ip netns exec red ip link set lo up

ip netns exec red ip link set vpeer0 up

ip netns exec red ip addr add 10.0.2.1/31 dev vpeer0

ip netns exec red ip route add default via 10.0.2.0
```

Likewise, define the 'blue' network.

```shell
ip netns add blue

ip link add veth1 type veth peer name vpeer1

ip link set veth1 up

ip addr add 10.0.1.0/31 dev veth1

ip link set vpeer1 netns blue

ip netns exec blue ip link set lo up

ip netns exec blue ip link set vpeer1 up

ip netns exec blue ip addr add 10.0.1.1/31 dev vpeer1

ip netns exec blue ip route add default via 10.0.1.0
```

Adjust the global network configuration

```shell
sysctl net.ipv4.ip_forward=1
```

Last, the mysqld binary needs extra permissions:

```shell
setcap cap_sys_admin+ep mysqld
```

This step, setcap, must be executed again everytime the mysqld binary is
rebuilt.

The location for 'mysqld' may vary, adjust by using the path to the
binary, for example:

```shell
[root@malff-desktop telemetry]# setcap cap_sys_admin+ep /home/malff/CODE/GIT/GIT_WL16735/build-dbg/runtime_output_directory/mysqld
```

### Verify the setup for named networks

In the global namespace, the network should look like this:

```shell
[malff@malff-desktop ~]$ ip address
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host 
       valid_lft forever preferred_lft forever
2: enp6s0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000
    link/ether 24:4b:fe:03:19:47 brd ff:ff:ff:ff:ff:ff
    inet 192.168.0.35/24 brd 192.168.0.255 scope global dynamic noprefixroute enp6s0
       valid_lft 39777sec preferred_lft 39777sec
    inet6 2a01:e0a:830:b7e0:264b:feff:fe03:1947/64 scope global dynamic noprefixroute 
       valid_lft 86340sec preferred_lft 86340sec
    inet6 fe80::264b:feff:fe03:1947/64 scope link noprefixroute 
       valid_lft forever preferred_lft forever
3: wlp5s0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc noqueue state DOWN group default qlen 1000
    link/ether fe:c4:38:0a:24:c9 brd ff:ff:ff:ff:ff:ff permaddr 3c:58:c2:20:9d:74
5: cscotun0: <POINTOPOINT,MULTICAST,NOARP,UP,LOWER_UP> mtu 1390 qdisc fq_codel state UNKNOWN group default qlen 500
    link/none 
    inet 10.154.144.123/20 brd 10.154.159.255 scope global cscotun0
       valid_lft forever preferred_lft forever
    inet6 fe80::d3cc:274d:9b87:2a09/126 scope link 
       valid_lft forever preferred_lft forever
    inet6 fe80::b13e:7f9e:d425:13b2/64 scope link stable-privacy 
       valid_lft forever preferred_lft forever
7: veth0@if6: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
    link/ether 56:c1:e7:a3:56:f0 brd ff:ff:ff:ff:ff:ff link-netns red
    inet 10.0.2.0/31 scope global veth0
       valid_lft forever preferred_lft forever
    inet6 fe80::54c1:e7ff:fea3:56f0/64 scope link 
       valid_lft forever preferred_lft forever
9: veth1@if8: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
    link/ether 66:81:f7:d7:d3:c3 brd ff:ff:ff:ff:ff:ff link-netns blue
    inet 10.0.1.0/31 scope global veth1
       valid_lft forever preferred_lft forever
    inet6 fe80::6481:f7ff:fed7:d3c3/64 scope link 
       valid_lft forever preferred_lft forever
```

In the 'red' namespace, the network should look like this:

```shell
[malff@malff-desktop ~]$ sudo ip netns exec red ip address
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host 
       valid_lft forever preferred_lft forever
6: vpeer0@if7: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
    link/ether 2e:23:5a:52:49:bd brd ff:ff:ff:ff:ff:ff link-netnsid 0
    inet 10.0.2.1/31 scope global vpeer0
       valid_lft forever preferred_lft forever
    inet6 fe80::2c23:5aff:fe52:49bd/64 scope link 
       valid_lft forever preferred_lft forever
```

In the 'blue' namespace, the network should look like this:

```shell
[malff@malff-desktop ~]$ sudo ip netns exec blue ip address
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host 
       valid_lft forever preferred_lft forever
8: vpeer1@if9: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
    link/ether 1e:05:3a:ed:15:e3 brd ff:ff:ff:ff:ff:ff link-netnsid 0
    inet 10.0.1.1/31 scope global vpeer1
       valid_lft forever preferred_lft forever
    inet6 fe80::1c05:3aff:feed:15e3/64 scope link 
       valid_lft forever preferred_lft forever
```

### Deploy an opentelemetry-collector in the global namespace

To deploy in the global namespace:

```shell
[malff@malff-desktop test]$ pwd
/home/malff/CODE/GIT/GIT_WL16735/components/telemetry/test
[malff@malff-desktop test]$ podman compose up -d
...
```

The collector endpoint will be on the default network, on port 4318.

In MySQL:

```
INSTALL COMPONENT 'file://component_telemetry'
  SET
    telemetry.otel_exporter_otlp_traces_endpoint='http://127.0.0.1:4318/v1/traces',
    telemetry.otel_exporter_otlp_metrics_endpoint='http://127.0.0.1:4318/v1/metrics',
    telemetry.otel_exporter_otlp_logs_endpoint='http://127.0.0.1:4318/v1/logs'
;
```

### Deploy an opentelemetry-collector in the red namespace

First, create a shell in the red network:

```shell
sudo ip netns exec red su - malff
```

Then, deploy within this shell:

```shell
podman compose up -d
```

The collector endpoint will be on the red network, on port 4318.

In MySQL:

```
INSTALL COMPONENT 'file://component_telemetry'
  SET
    telemetry.otel_exporter_otlp_traces_network_namespace= 'red',
    telemetry.otel_exporter_otlp_metrics_network_namespace='red',
    telemetry.otel_exporter_otlp_logs_network_namespace= 'red',
    telemetry.otel_exporter_otlp_traces_endpoint='http://10.0.2.1:4318/v1/traces',
    telemetry.otel_exporter_otlp_metrics_endpoint='http://10.0.2.1:4318/v1/metrics',
    telemetry.otel_exporter_otlp_logs_endpoint='http://10.0.2.1:4318/v1/logs'
;
```
