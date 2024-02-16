# ==== Purpose ====
#
# Suppress tls off messages
#
# ==== Usage ====
#
# --source include/suppress_tls_off.inc

let $messages = .*Failed to initialize TLS for channel: [a-z_]*. See below for the description of exact issue.*
.*Failed to set up SSL because of the following SSL library error: TLS version is invalid.*
;
let $suppress_silent=1;
--source include/suppress_messages.inc
