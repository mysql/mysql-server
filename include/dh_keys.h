/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 *
 * Contains Diffie-Hellman Keys.
 * Positioned in the array as per the security level.
 *
 * +------------------------------+
 * | sec-level |   min-key-size   |
 * +-----------+------------------+
 * |         1 |             1024 |
 * |         2 |             2048 |
 * |         3 |             3072 |
 * |         4 |             7680 |
 * |         5 |            15360 |
 * +------------------------------+
 *
 * Minimum key size for security level 0 and 1
 * should be 2048.
 *
 */

#ifndef DH_KEYS_INCLUDED
#define DH_KEYS_INCLUDED

#include <array>
#include <string_view>

namespace {
constexpr const std::array<std::string_view, 6> dh_keys{
    /*
       Diffie-Hellman key.
       Generated using: >openssl dhparam -5 -C 2048
    */
    "-----BEGIN DH PARAMETERS-----\n"
    "MIIBCAKCAQEAil36wGZ2TmH6ysA3V1xtP4MKofXx5n88xq/aiybmGnReZMviCPEJ\n"
    "46+7VCktl/RZ5iaDH1XNG1dVQmznt9pu2G3usU+k1/VB4bQL4ZgW4u0Wzxh9PyXD\n"
    "glm99I9Xyj4Z5PVE4MyAsxCRGA1kWQpD9/zKAegUBPLNqSo886Uqg9hmn8ksyU9E\n"
    "BV5eAEciCuawh6V0O+Sj/C3cSfLhgA0GcXp3OqlmcDu6jS5gWjn3LdP1U0duVxMB\n"
    "h/neTSCSvtce4CAMYMjKNVh9P1nu+2d9ZH2Od2xhRIqMTfAS1KTqF3VmSWzPFCjG\n"
    "mjxx/bg6bOOjpgZapvB6ABWlWmRmAAWFtwIBBQ==\n"
    "-----END DH PARAMETERS-----",

    /*
       Diffie-Hellman key.
       Generated using: >openssl dhparam -5 -C 2048
    */
    "-----BEGIN DH PARAMETERS-----\n"
    "MIIBCAKCAQEAil36wGZ2TmH6ysA3V1xtP4MKofXx5n88xq/aiybmGnReZMviCPEJ\n"
    "46+7VCktl/RZ5iaDH1XNG1dVQmznt9pu2G3usU+k1/VB4bQL4ZgW4u0Wzxh9PyXD\n"
    "glm99I9Xyj4Z5PVE4MyAsxCRGA1kWQpD9/zKAegUBPLNqSo886Uqg9hmn8ksyU9E\n"
    "BV5eAEciCuawh6V0O+Sj/C3cSfLhgA0GcXp3OqlmcDu6jS5gWjn3LdP1U0duVxMB\n"
    "h/neTSCSvtce4CAMYMjKNVh9P1nu+2d9ZH2Od2xhRIqMTfAS1KTqF3VmSWzPFCjG\n"
    "mjxx/bg6bOOjpgZapvB6ABWlWmRmAAWFtwIBBQ==\n"
    "-----END DH PARAMETERS-----",

    /*
       Diffie-Hellman key.
       Generated using: >openssl dhparam -5 -C 2048
    */
    "-----BEGIN DH PARAMETERS-----\n"
    "MIIBCAKCAQEAil36wGZ2TmH6ysA3V1xtP4MKofXx5n88xq/aiybmGnReZMviCPEJ\n"
    "46+7VCktl/RZ5iaDH1XNG1dVQmznt9pu2G3usU+k1/VB4bQL4ZgW4u0Wzxh9PyXD\n"
    "glm99I9Xyj4Z5PVE4MyAsxCRGA1kWQpD9/zKAegUBPLNqSo886Uqg9hmn8ksyU9E\n"
    "BV5eAEciCuawh6V0O+Sj/C3cSfLhgA0GcXp3OqlmcDu6jS5gWjn3LdP1U0duVxMB\n"
    "h/neTSCSvtce4CAMYMjKNVh9P1nu+2d9ZH2Od2xhRIqMTfAS1KTqF3VmSWzPFCjG\n"
    "mjxx/bg6bOOjpgZapvB6ABWlWmRmAAWFtwIBBQ==\n"
    "-----END DH PARAMETERS-----",

    /*
       Diffie-Hellman key.
       Generated using: >openssl dhparam -5 -C 3072
    */
    "-----BEGIN DH PARAMETERS-----\n"
    "MIIBiAKCAYEA9sjCgCPIir/lzSpWNH4VfgSp+j2/0oJpUuF9U3m5GDCc8j1CBXvT\n"
    "utOtuysTXYXtcbsHwVAwM0QD1iFThIfeI1omUFUuCiOir3zHJCNfPu2K6qRb4avz\n"
    "TxXf5Lco9UpPxE4vYgMGMdV41y0N+U3uocQ/S76BPKxQebr3euNAwqupn2jjrEbd\n"
    "Ensi5kB25wcxFUTAqziArcseE/UDgCFCmIg3UUaQIWKpiOpxJGR/4dBFlU1l7jtn\n"
    "kGlKR0LuKkmhP8y3nFzJqpP2ItGUupkUpseleSStl2rgaUGG8sdNpJNWGZq9wJcu\n"
    "+KVB5BwdpijM+eCAMhUXae7H3ShvyD/GQnc1ugHGHN9/Rtu88z9KOGEsn98W6KjD\n"
    "AMiQUXzNLWCtBkWpYheGOAAcgZgtAFtoUJ/aKztx6V+tS/5C4yr2K7OhJ157enax\n"
    "gvbM01aAN2xlEglmGdxEKhlRFdrgBVw3R8qHwwX2m8QFV15nwl2ZVBr7hC+cMx9A\n"
    "c1cfbUBAIe0/AgEF\n"
    "-----END DH PARAMETERS-----",

    /*
       Diffie-Hellman key.
       Generated using: >openssl dhparam -5 -C 7680
    */
    "-----BEGIN DH PARAMETERS-----\n"
    "MIIDyAKCA8EAqs4RjZyRCVK5Cy19YqdCBg3zIy7fuBOjA19D1dtVBBGvHxvGpY91\n"
    "B44SlbxfYBYdJjm9xGR0fV5PRjHqmbSg0e9y7+rFVtKFLjVMLO02/ywgHAl4iohe\n"
    "1RwXilxInXiEhxhvsdfvyHEK+JJNo+wSprfJXZ4jN/YnTWgQQZK2n8QnVJFaZK0n\n"
    "Thg8fQtFH4+oXSUNY+ad8xM5qx6lo2UU/bc4CoE0/FqE0OFIrwCcOotOs0t8NyeG\n"
    "vY7uhezaEmJVTGZzIVJbb/Qc/w1dFzlWvgA8mIP8cs1Y/FfvcxYCWi0B34M0ZQlE\n"
    "eScFq/fMkLq/gWagvXYtmu+Mb6BGghfhpHbhw+cNkXMIMLsHl8EnXfgHsAR4I3gE\n"
    "sGsOEu59e0aWpImhaYJWBpRHeaLKpLalqwzhYrVFvvhJ8wLXzvuKTyuvUWLrafAy\n"
    "M26zIxHu+jUTanWY8BI9GRhJ8Cqt9DCvRUt0+CMkvWoxK0DqWoOT09oAYrlHnL1/\n"
    "U3iJ7oMgNFNG3JSfDOI7rWUu04FPMzLx+Ue72TuaLqDau6KsEyBtREdNNwEZTuCT\n"
    "WJc99b9tl+VZX5uEib1iWKbLJWVgjg1VDB1bJaiNpedsN+l6x7Ia37OtwMJI+KCS\n"
    "NHiWGIsPyYESq/gBX7Sb8QK20TF9Gz1I73pykrntU4O209htfrSKRFu+A6dG8AjG\n"
    "jvnILhw5X2YEevH+O+2kj6hlbF5Ztx/yqc6h6+hYTo7xxNUNVDZp0b9VeXmzWnjK\n"
    "LphMsn6QGi4mF/rtSoLDWqRlrA2oZPwK3bAUjTS1Xlfwn2SOkU/NNbg4MvFIU0up\n"
    "isRZmxkAmPXUv8nAk8kosB9svNPg6Us0iA1wT/t/bW5KAf61KEcX8YqN9jLCx2Fo\n"
    "mLu4C9dKPcfteDni4/KrdhTSO7wE39CwS9MjuaJNiZy3gxn8u9LgLGp8syCnN4WL\n"
    "qPgBqsi78OlAAURMf/xhCNNg2Wm9a5b52qK9q422AwYKoJezlonBoAe7DfnBbbXp\n"
    "TiaJsOBbHqc9xlyMhz1gQqZVwENGYtyT1U1syd/mWpwNciVTsfKk9lfmja7x4xNc\n"
    "w4jFNYykhIaYJZXUCTP/1TDFd6hRCpLq1MMaVJwUKASa8dcbjXPmXjW/ZavHqtHv\n"
    "0//Isr/QZjhk3VmfRGSlOJ7NQsx7MXgcJ8KCwvZVjz8D9SIg+6UMmbJxHXrO5jUW\n"
    "0+L4TWN6ZmBlkW+vTrEDZvlhGOHcutmPvTsn+DL1twybjzQ7UNqQMKS3HsoETEMi\n"
    "Qx7+mW52N5RvAgEF\n"
    "-----END DH PARAMETERS-----",

    /*
       Diffie-Hellman key.
       Generated using: >openssl dhparam -5 -C 15360
    */
    "-----BEGIN DH PARAMETERS-----\n"
    "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\n"
    "3Zk3+OFVkhFDAgEF\n"
    "-----END DH PARAMETERS-----"};
}  // namespace

#endif
