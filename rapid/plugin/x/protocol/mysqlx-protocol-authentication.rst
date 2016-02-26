.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

Authentication
==============

Authentication is implemented according to :rfc:`4422` (SASL):

service-name
  ``mysql`` (see http://www.iana.org/assignments/gssapi-service-names/gssapi-service-names.xhtml)

mechanism-negotiation
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`

messages
  1. :protobuf:msg:`Mysqlx.Session::AuthenticateStart`
  2. :protobuf:msg:`Mysqlx.Session::AuthenticateContinue`
  3. :protobuf:msg:`Mysqlx::Error`
  4. :protobuf:msg:`Mysqlx.Session::AuthenticateOk`

PLAIN authentication
....................

.. code-block:: cucumber

  Feature: Authentication

    Scenario: plaintext auth with over TLS
      Given TLS connection is established
      And "PLAIN" auth is supported
      And username "foo" and password "bar" are valid
      When authenticating with "PLAIN" and username "foo" and password "bar"
      Then authentication succeed
      And takes only one round trip

.. uml::

  == Authentication ==

  Client -> Server: AuthenticateStart(mech="PLAIN", auth_data="\0foo\0bar")
  Server --> Client: Ok

EXTERNAL authentication
.......................

.. code-block:: cucumber

    Scenario: authenticate with a X.509 client certificate
      Given TLS connection is established
      And EXTERNAL auth is support
      And X.509 client certificate contains valid username
      When authenticating with "EXTERNAL"
      Then authentication succeeds
      And takes only one round trip

.. uml::

  == Authentication ==

  Client -> Server: AuthenticateStart(mech="EXTERNAL", initial_response="")
  Server --> Client: AuthenticateOk

MYSQL41 authentication
......................

* Supported by MySQL 4.1 and later
* challenge/response protocol using SHA1
* similar to CRAM-MD5 (:rfc:`2195`)

::

  C: user
  S: 20 bytes challenge
  C: SHA1(password ^ SHA1(challenge + SHA1(SHA1(password))))
  S: Ok

.. uml::

  == Authentication ==

  Client -> Server: AuthenticateStart(mech="MYSQL41", auth_data="username")
  Server --> Client: AuthenticateContinue(auth_data="<random-printable>*20"
  Client -> Server: AuthenticateContinue(auth_data="<scrambled-password>")
  Server --> Client: AuthenticateOk

SCRAM authentication
....................

* mutual authentication
* supports channel binding
* replacable hashing algorithm
* salted passwords with hashing iterations

::

  C: n,,n=...,r=...    // no channel binding, no authzid, authname, c-nonce
  S: r=...,s=...,i=... // c-none + s-nonce, salt, iter-count
  C: c=...,r=...,p=... // base64(no-channel binding, no authzid), c-once + s-nonce, proof
  S: v=...             // server verifier

