/*
   Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
*/

#define Copyright yaCopyright
#define yaSSL_CleanUp yayaSSL_CleanUp
#define BN_bin2bn yaBN_bin2bn
#define DH_new yaDH_new
#define DH_free yaDH_free
#define RSA_free yaRSA_free
#define RSA_generate_key yaRSA_generate_key
#define X509_free yaX509_free
#define X509_STORE_CTX_get_current_cert yaX509_STORE_CTX_get_current_cert
#define X509_STORE_CTX_get_error yaX509_STORE_CTX_get_error
#define X509_STORE_CTX_get_error_depth yaX509_STORE_CTX_get_error_depth
#define X509_NAME_oneline yaX509_NAME_oneline
#define X509_get_issuer_name yaX509_get_issuer_name
#define X509_get_subject_name yaX509_get_subject_name
#define X509_verify_cert_error_string yaX509_verify_cert_error_string
#define X509_LOOKUP_add_dir yaX509_LOOKUP_add_dir
#define X509_LOOKUP_load_file yaX509_LOOKUP_load_file
#define X509_LOOKUP_hash_dir yaX509_LOOKUP_hash_dir
#define X509_LOOKUP_file yaX509_LOOKUP_file
#define X509_STORE_add_lookup yaX509_STORE_add_lookup
#define X509_STORE_new yaX509_STORE_new
#define X509_STORE_get_by_subject yaX509_STORE_get_by_subject
#define ERR_get_error_line_data yaERR_get_error_line_data
#define ERR_print_errors_fp yaERR_print_errors_fp
#define ERR_error_string yaERR_error_string
#define ERR_remove_state yaERR_remove_state
#define ERR_get_error yaERR_get_error
#define ERR_peek_error yaERR_peek_error
#define ERR_GET_REASON yaERR_GET_REASON
#define SSL_CTX_new yaSSL_CTX_new
#define SSL_new yaSSL_new
#define SSL_set_fd yaSSL_set_fd
#define SSL_get_fd yaSSL_get_fd
#define SSL_connect yaSSL_connect
#define SSL_write yaSSL_write
#define SSL_read yaSSL_read
#define SSL_accept yaSSL_accept
#define SSL_CTX_free yaSSL_CTX_free
#define SSL_free yaSSL_free
#define SSL_clear yaSSL_clear
#define SSL_shutdown yaSSL_shutdown
#define SSL_set_connect_state yaSSL_set_connect_state
#define SSL_set_accept_state yaSSL_set_accept_state
#define SSL_do_handshake yaSSL_do_handshake
#define SSL_get_cipher yaSSL_get_cipher
#define SSL_get_cipher_name yaSSL_get_cipher_name
#define SSL_get_shared_ciphers yaSSL_get_shared_ciphers
#define SSL_get_cipher_list yaSSL_get_cipher_list
#define SSL_get_version yaSSL_get_version
#define SSLeay_version yaSSLeay_version
#define SSL_get_error yaSSL_get_error
#define SSL_load_error_strings yaSSL_load_error_strings
#define SSL_set_session yaSSL_set_session
#define SSL_get_session yaSSL_get_session
#define SSL_flush_sessions yaSSL_flush_sessions
#define SSL_SESSION_set_timeout yaSSL_SESSION_set_timeout
#define SSL_CTX_set_session_cache_mode yaSSL_CTX_set_session_cache_mode
#define SSL_get_peer_certificate yaSSL_get_peer_certificate
#define SSL_get_verify_result yaSSL_get_verify_result
#define SSL_CTX_set_verify yaSSL_CTX_set_verify
#define SSL_CTX_load_verify_locations yaSSL_CTX_load_verify_locations
#define SSL_CTX_set_default_verify_paths yaSSL_CTX_set_default_verify_paths
#define SSL_CTX_check_private_key yaSSL_CTX_check_private_key
#define SSL_CTX_set_session_id_context yaSSL_CTX_set_session_id_context
#define SSL_CTX_set_tmp_rsa_callback yaSSL_CTX_set_tmp_rsa_callback
#define SSL_CTX_set_options yaSSL_CTX_set_options
#define SSL_CTX_set_session_cache_mode yaSSL_CTX_set_session_cache_mode
#define SSL_CTX_set_timeout yaSSL_CTX_set_timeout
#define SSL_CTX_use_certificate_chain_file yaSSL_CTX_use_certificate_chain_file
#define SSL_CTX_set_default_passwd_cb yaSSL_CTX_set_default_passwd_cb
#define SSL_CTX_use_RSAPrivateKey_file yaSSL_CTX_use_RSAPrivateKey_file
#define SSL_CTX_set_info_callback yaSSL_CTX_set_info_callback
#define SSL_CTX_sess_accept yaSSL_CTX_sess_accept
#define SSL_CTX_sess_connect yaSSL_CTX_sess_connect
#define SSL_CTX_sess_accept_good yaSSL_CTX_sess_accept_good
#define SSL_CTX_sess_connect_good yaSSL_CTX_sess_connect_good
#define SSL_CTX_sess_accept_renegotiate yaSSL_CTX_sess_accept_renegotiate
#define SSL_CTX_sess_connect_renegotiate yaSSL_CTX_sess_connect_renegotiate
#define SSL_CTX_sess_hits yaSSL_CTX_sess_hits
#define SSL_CTX_sess_cb_hits yaSSL_CTX_sess_cb_hits
#define SSL_CTX_sess_cache_full yaSSL_CTX_sess_cache_full
#define SSL_CTX_sess_misses yaSSL_CTX_sess_misses
#define SSL_CTX_sess_timeouts yaSSL_CTX_sess_timeouts
#define SSL_CTX_sess_number yaSSL_CTX_sess_number
#define SSL_CTX_sess_get_cache_size yaSSL_CTX_sess_get_cache_size
#define SSL_CTX_get_verify_mode yaSSL_CTX_get_verify_mode
#define SSL_get_verify_mode yaSSL_get_verify_mode
#define SSL_CTX_get_verify_depth yaSSL_CTX_get_verify_depth
#define SSL_get_verify_depth yaSSL_get_verify_depth
#define SSL_get_default_timeout yaSSL_get_default_timeout
#define SSL_CTX_get_session_cache_mode yaSSL_CTX_get_session_cache_mode
#define SSL_session_reused yaSSL_session_reused
#define SSL_set_rfd yaSSL_set_rfd
#define SSL_set_wfd yaSSL_set_wfd
#define SSL_set_shutdown yaSSL_set_shutdown
#define SSL_set_quiet_shutdown yaSSL_set_quiet_shutdown
#define SSL_get_quiet_shutdown yaSSL_get_quiet_shutdown
#define SSL_want_read yaSSL_want_read
#define SSL_want_write yaSSL_want_write
#define SSL_pending yaSSL_pending
#define SSLv3_method yaSSLv3_method
#define SSLv3_server_method yaSSLv3_server_method
#define SSLv3_client_method yaSSLv3_client_method
#define TLSv1_server_method yaTLSv1_server_method
#define TLSv1_client_method yaTLSv1_client_method
#define TLSv1_1_server_method yaTLSv1_1_server_method
#define TLSv1_1_client_method yaTLSv1_1_client_method
#define SSLv23_server_method yaSSLv23_server_method
#define SSL_CTX_use_certificate_file yaSSL_CTX_use_certificate_file
#define SSL_CTX_use_PrivateKey_file yaSSL_CTX_use_PrivateKey_file
#define SSL_CTX_set_cipher_list yaSSL_CTX_set_cipher_list
#define SSL_CTX_sess_set_cache_size yaSSL_CTX_sess_set_cache_size
#define SSL_CTX_set_tmp_dh yaSSL_CTX_set_tmp_dh
#define OpenSSL_add_all_algorithms yaOpenSSL_add_all_algorithms
#define SSL_library_init yaSSL_library_init
#define SSLeay_add_ssl_algorithms yaSSLeay_add_ssl_algorithms
#define SSL_get_current_cipher yaSSL_get_current_cipher
#define SSL_CIPHER_description yaSSL_CIPHER_description
#define SSL_alert_type_string_long yaSSL_alert_type_string_long
#define SSL_alert_desc_string_long yaSSL_alert_desc_string_long
#define SSL_state_string_long yaSSL_state_string_long
#define EVP_md5 yaEVP_md5
#define EVP_des_ede3_cbc yaEVP_des_ede3_cbc
#define EVP_BytesToKey yaEVP_BytesToKey
#define DES_set_key_unchecked yaDES_set_key_unchecked
#define DES_ede3_cbc_encrypt yaDES_ede3_cbc_encrypt
#define RAND_screen yaRAND_screen
#define RAND_file_name yaRAND_file_name
#define RAND_write_file yaRAND_write_file
#define RAND_load_file yaRAND_load_file
#define RAND_status yaRAND_status
#define RAND_bytes yaRAND_bytes
#define DES_set_key yaDES_set_key
#define DES_set_odd_parity yaDES_set_odd_parity
#define DES_ecb_encrypt yaDES_ecb_encrypt
#define SSL_CTX_set_default_passwd_cb_userdata yaSSL_CTX_set_default_passwd_cb_userdata
#define SSL_SESSION_free yaSSL_SESSION_free
#define SSL_peek yaSSL_peek
#define SSL_get_certificate yaSSL_get_certificate
#define SSL_get_privatekey yaSSL_get_privatekey
#define X509_get_pubkey yaX509_get_pubkey
#define EVP_PKEY_copy_parameters yaEVP_PKEY_copy_parameters
#define EVP_PKEY_free yaEVP_PKEY_free
#define ERR_error_string_n yaERR_error_string_n
#define ERR_free_strings yaERR_free_strings
#define EVP_cleanup yaEVP_cleanup
#define X509_get_ext_d2i yaX509_get_ext_d2i
#define GENERAL_NAMES_free yaGENERAL_NAMES_free
#define sk_GENERAL_NAME_num yask_GENERAL_NAME_num
#define sk_GENERAL_NAME_value yask_GENERAL_NAME_value
#define ASN1_STRING_data yaASN1_STRING_data
#define ASN1_STRING_length yaASN1_STRING_length
#define ASN1_STRING_type yaASN1_STRING_type
#define X509_NAME_get_index_by_NID yaX509_NAME_get_index_by_NID
#define X509_NAME_ENTRY_get_data yaX509_NAME_ENTRY_get_data
#define X509_NAME_get_entry yaX509_NAME_get_entry
#define ASN1_STRING_to_UTF8 yaASN1_STRING_to_UTF8
#define SSLv23_client_method yaSSLv23_client_method
#define SSLv2_client_method yaSSLv2_client_method
#define SSL_get1_session yaSSL_get1_session
#define X509_get_notBefore yaX509_get_notBefore
#define X509_get_notAfter yaX509_get_notAfter
#define yaSSL_ASN1_TIME_to_string ya_SSL_ASN1_TIME_to_string
#define MD4_Init yaMD4_Init
#define MD4_Update yaMD4_Update
#define MD4_Final yaMD4_Final
#define MD5_Init yaMD5_Init
#define MD5_Update yaMD5_Update
#define MD5_Final yaMD5_Final
#define SSL_set_compression yaSSL_set_compression
#define PEM_read_X509 yaSSL_PEM_read_X509
