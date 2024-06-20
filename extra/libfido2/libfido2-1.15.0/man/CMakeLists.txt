# Copyright (c) 2018-2022 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

find_program(MANDOC_PATH mandoc)
find_program(GZIP_PATH gzip)

message(STATUS "MANDOC_PATH: ${MANDOC_PATH}")
message(STATUS "GZIP_PATH: ${GZIP_PATH}")

list(APPEND MAN_SOURCES
	eddsa_pk_new.3
	es256_pk_new.3
	es384_pk_new.3
	fido2-assert.1
	fido2-cred.1
	fido2-token.1
	fido_init.3
	fido_assert_new.3
	fido_assert_allow_cred.3
	fido_assert_set_authdata.3
	fido_assert_verify.3
	fido_bio_dev_get_info.3
	fido_bio_enroll_new.3
	fido_bio_info_new.3
	fido_bio_template.3
	fido_cbor_info_new.3
	fido_cred_new.3
	fido_cred_exclude.3
	fido_credman_metadata_new.3
	fido_cred_set_authdata.3
	fido_cred_verify.3
	fido_dev_enable_entattest.3
	fido_dev_get_assert.3
	fido_dev_get_touch_begin.3
	fido_dev_info_manifest.3
	fido_dev_largeblob_get.3
	fido_dev_make_cred.3
	fido_dev_open.3
	fido_dev_set_io_functions.3
	fido_dev_set_pin.3
	fido_strerr.3
	rs256_pk_new.3
)

list(APPEND MAN_ALIAS
	eddsa_pk_new eddsa_pk_free
	eddsa_pk_new eddsa_pk_from_EVP_PKEY
	eddsa_pk_new eddsa_pk_from_ptr
	eddsa_pk_new eddsa_pk_to_EVP_PKEY
	es256_pk_new es256_pk_free
	es256_pk_new es256_pk_from_EC_KEY
	es256_pk_new es256_pk_from_EVP_PKEY
	es256_pk_new es256_pk_from_ptr
	es256_pk_new es256_pk_to_EVP_PKEY
	es384_pk_new es384_pk_free
	es384_pk_new es384_pk_from_EC_KEY
	es384_pk_new es384_pk_from_EVP_PKEY
	es384_pk_new es384_pk_from_ptr
	es384_pk_new es384_pk_to_EVP_PKEY
	fido_assert_allow_cred fido_assert_empty_allow_list
	fido_assert_new fido_assert_authdata_len
	fido_assert_new fido_assert_authdata_ptr
	fido_assert_new fido_assert_authdata_raw_len
	fido_assert_new fido_assert_authdata_raw_ptr
	fido_assert_new fido_assert_blob_len
	fido_assert_new fido_assert_blob_ptr
	fido_assert_new fido_assert_clientdata_hash_len
	fido_assert_new fido_assert_clientdata_hash_ptr
	fido_assert_new fido_assert_count
	fido_assert_new fido_assert_flags
	fido_assert_new fido_assert_free
	fido_assert_new fido_assert_hmac_secret_len
	fido_assert_new fido_assert_hmac_secret_ptr
	fido_assert_new fido_assert_id_len
	fido_assert_new fido_assert_id_ptr
	fido_assert_new fido_assert_largeblob_key_len
	fido_assert_new fido_assert_largeblob_key_ptr
	fido_assert_new fido_assert_rp_id
	fido_assert_new fido_assert_sigcount
	fido_assert_new fido_assert_sig_len
	fido_assert_new fido_assert_sig_ptr
	fido_assert_new fido_assert_user_display_name
	fido_assert_new fido_assert_user_icon
	fido_assert_new fido_assert_user_id_len
	fido_assert_new fido_assert_user_id_ptr
	fido_assert_new fido_assert_user_name
	fido_assert_set_authdata fido_assert_set_authdata_raw
	fido_assert_set_authdata fido_assert_set_clientdata
	fido_assert_set_authdata fido_assert_set_clientdata_hash
	fido_assert_set_authdata fido_assert_set_count
	fido_assert_set_authdata fido_assert_set_extensions
	fido_assert_set_authdata fido_assert_set_hmac_salt
	fido_assert_set_authdata fido_assert_set_hmac_secret
	fido_assert_set_authdata fido_assert_set_rp
	fido_assert_set_authdata fido_assert_set_sig
	fido_assert_set_authdata fido_assert_set_up
	fido_assert_set_authdata fido_assert_set_uv
	fido_assert_set_authdata fido_assert_set_winhello_appid
	fido_bio_dev_get_info fido_bio_dev_enroll_begin
	fido_bio_dev_get_info fido_bio_dev_enroll_cancel
	fido_bio_dev_get_info fido_bio_dev_enroll_continue
	fido_bio_dev_get_info fido_bio_dev_enroll_remove
	fido_bio_dev_get_info fido_bio_dev_get_template_array
	fido_bio_dev_get_info fido_bio_dev_set_template_name
	fido_bio_enroll_new fido_bio_enroll_free
	fido_bio_enroll_new fido_bio_enroll_last_status
	fido_bio_enroll_new fido_bio_enroll_remaining_samples
	fido_bio_info_new fido_bio_info_free
	fido_bio_info_new fido_bio_info_max_samples
	fido_bio_info_new fido_bio_info_type
	fido_bio_template fido_bio_template_array_count
	fido_bio_template fido_bio_template_array_free
	fido_bio_template fido_bio_template_array_new
	fido_bio_template fido_bio_template_free
	fido_bio_template fido_bio_template_id_len
	fido_bio_template fido_bio_template_id_ptr
	fido_bio_template fido_bio_template_name
	fido_bio_template fido_bio_template_new
	fido_bio_template fido_bio_template_set_id
	fido_bio_template fido_bio_template_set_name
	fido_cbor_info_new fido_cbor_info_aaguid_len
	fido_cbor_info_new fido_cbor_info_aaguid_ptr
	fido_cbor_info_new fido_cbor_info_algorithm_cose
	fido_cbor_info_new fido_cbor_info_algorithm_count
	fido_cbor_info_new fido_cbor_info_algorithm_type
	fido_cbor_info_new fido_cbor_info_certs_len
	fido_cbor_info_new fido_cbor_info_certs_name_ptr
	fido_cbor_info_new fido_cbor_info_certs_value_ptr
	fido_cbor_info_new fido_cbor_info_extensions_len
	fido_cbor_info_new fido_cbor_info_extensions_ptr
	fido_cbor_info_new fido_cbor_info_free
	fido_cbor_info_new fido_cbor_info_fwversion
	fido_cbor_info_new fido_cbor_info_maxcredbloblen
	fido_cbor_info_new fido_cbor_info_maxcredcntlst
	fido_cbor_info_new fido_cbor_info_maxcredidlen
	fido_cbor_info_new fido_cbor_info_maxlargeblob
	fido_cbor_info_new fido_cbor_info_maxmsgsiz
	fido_cbor_info_new fido_cbor_info_maxrpid_minpinlen
	fido_cbor_info_new fido_cbor_info_minpinlen
	fido_cbor_info_new fido_cbor_info_new_pin_required
	fido_cbor_info_new fido_cbor_info_options_len
	fido_cbor_info_new fido_cbor_info_options_name_ptr
	fido_cbor_info_new fido_cbor_info_options_value_ptr
	fido_cbor_info_new fido_cbor_info_protocols_len
	fido_cbor_info_new fido_cbor_info_protocols_ptr
	fido_cbor_info_new fido_cbor_info_rk_remaining
	fido_cbor_info_new fido_cbor_info_transports_len
	fido_cbor_info_new fido_cbor_info_transports_ptr
	fido_cbor_info_new fido_cbor_info_uv_attempts
	fido_cbor_info_new fido_cbor_info_uv_modality
	fido_cbor_info_new fido_cbor_info_versions_len
	fido_cbor_info_new fido_cbor_info_versions_ptr
	fido_cbor_info_new fido_dev_get_cbor_info
	fido_cred_exclude fido_cred_empty_exclude_list
	fido_cred_new fido_cred_aaguid_len
	fido_cred_new fido_cred_aaguid_ptr
	fido_cred_new fido_cred_attstmt_len
	fido_cred_new fido_cred_attstmt_ptr
	fido_cred_new fido_cred_authdata_len
	fido_cred_new fido_cred_authdata_ptr
	fido_cred_new fido_cred_authdata_raw_len
	fido_cred_new fido_cred_authdata_raw_ptr
	fido_cred_new fido_cred_clientdata_hash_len
	fido_cred_new fido_cred_clientdata_hash_ptr
	fido_cred_new fido_cred_display_name
	fido_cred_new fido_cred_flags
	fido_cred_new fido_cred_fmt
	fido_cred_new fido_cred_free
	fido_cred_new fido_cred_id_len
	fido_cred_new fido_cred_id_ptr
	fido_cred_new fido_cred_largeblob_key_len
	fido_cred_new fido_cred_largeblob_key_ptr
	fido_cred_new fido_cred_pin_minlen
	fido_cred_new fido_cred_prot
	fido_cred_new fido_cred_pubkey_len
	fido_cred_new fido_cred_pubkey_ptr
	fido_cred_new fido_cred_rp_id
	fido_cred_new fido_cred_rp_name
	fido_cred_new fido_cred_sigcount
	fido_cred_new fido_cred_sig_len
	fido_cred_new fido_cred_sig_ptr
	fido_cred_new fido_cred_type
	fido_cred_new fido_cred_user_id_len
	fido_cred_new fido_cred_user_id_ptr
	fido_cred_new fido_cred_user_name
	fido_cred_new fido_cred_x5c_len
	fido_cred_new fido_cred_x5c_list_count
	fido_cred_new fido_cred_x5c_list_len
	fido_cred_new fido_cred_x5c_list_ptr
	fido_cred_new fido_cred_x5c_ptr
	fido_cred_verify fido_cred_verify_self
	fido_credman_metadata_new fido_credman_del_dev_rk
	fido_credman_metadata_new fido_credman_get_dev_metadata
	fido_credman_metadata_new fido_credman_get_dev_rk
	fido_credman_metadata_new fido_credman_get_dev_rp
	fido_credman_metadata_new fido_credman_metadata_free
	fido_credman_metadata_new fido_credman_rk
	fido_credman_metadata_new fido_credman_rk_count
	fido_credman_metadata_new fido_credman_rk_existing
	fido_credman_metadata_new fido_credman_rk_free
	fido_credman_metadata_new fido_credman_rk_new
	fido_credman_metadata_new fido_credman_rk_remaining
	fido_credman_metadata_new fido_credman_rp_count
	fido_credman_metadata_new fido_credman_rp_free
	fido_credman_metadata_new fido_credman_rp_id
	fido_credman_metadata_new fido_credman_rp_id_hash_len
	fido_credman_metadata_new fido_credman_rp_id_hash_ptr
	fido_credman_metadata_new fido_credman_rp_name
	fido_credman_metadata_new fido_credman_rp_new
	fido_credman_metadata_new fido_credman_set_dev_rk
	fido_cred_set_authdata fido_cred_set_attstmt
	fido_cred_set_authdata fido_cred_set_attobj
	fido_cred_set_authdata fido_cred_set_authdata_raw
	fido_cred_set_authdata fido_cred_set_blob
	fido_cred_set_authdata fido_cred_set_clientdata
	fido_cred_set_authdata fido_cred_set_clientdata_hash
	fido_cred_set_authdata fido_cred_set_extensions
	fido_cred_set_authdata fido_cred_set_fmt
	fido_cred_set_authdata fido_cred_set_id
	fido_cred_set_authdata fido_cred_set_pin_minlen
	fido_cred_set_authdata fido_cred_set_prot
	fido_cred_set_authdata fido_cred_set_rk
	fido_cred_set_authdata fido_cred_set_rp
	fido_cred_set_authdata fido_cred_set_sig
	fido_cred_set_authdata fido_cred_set_type
	fido_cred_set_authdata fido_cred_set_user
	fido_cred_set_authdata fido_cred_set_uv
	fido_cred_set_authdata fido_cred_set_x509
	fido_dev_enable_entattest fido_dev_toggle_always_uv
	fido_dev_enable_entattest fido_dev_force_pin_change
	fido_dev_enable_entattest fido_dev_set_pin_minlen
	fido_dev_enable_entattest fido_dev_set_pin_minlen_rpid
	fido_dev_get_touch_begin fido_dev_get_touch_status
	fido_dev_info_manifest fido_dev_info_free
	fido_dev_info_manifest fido_dev_info_manufacturer_string
	fido_dev_info_manifest fido_dev_info_new
	fido_dev_info_manifest fido_dev_info_path
	fido_dev_info_manifest fido_dev_info_product
	fido_dev_info_manifest fido_dev_info_product_string
	fido_dev_info_manifest fido_dev_info_ptr
	fido_dev_info_manifest fido_dev_info_set
	fido_dev_info_manifest fido_dev_info_vendor
	fido_dev_open fido_dev_build
	fido_dev_open fido_dev_cancel
	fido_dev_open fido_dev_close
	fido_dev_open fido_dev_flags
	fido_dev_open fido_dev_force_fido2
	fido_dev_open fido_dev_force_u2f
	fido_dev_open fido_dev_free
	fido_dev_open fido_dev_has_pin
	fido_dev_open fido_dev_has_uv
	fido_dev_open fido_dev_is_fido2
	fido_dev_open fido_dev_is_winhello
	fido_dev_open fido_dev_major
	fido_dev_open fido_dev_minor
	fido_dev_open fido_dev_new
	fido_dev_open fido_dev_new_with_info
	fido_dev_open fido_dev_open_with_info
	fido_dev_open fido_dev_protocol
	fido_dev_open fido_dev_supports_cred_prot
	fido_dev_open fido_dev_supports_credman
	fido_dev_open fido_dev_supports_permissions
	fido_dev_open fido_dev_supports_pin
	fido_dev_open fido_dev_supports_uv
	fido_dev_set_pin fido_dev_get_retry_count
	fido_dev_set_pin fido_dev_get_uv_retry_count
	fido_dev_set_pin fido_dev_reset
	fido_dev_set_io_functions fido_dev_io_handle
	fido_dev_set_io_functions fido_dev_set_sigmask
	fido_dev_set_io_functions fido_dev_set_timeout
	fido_dev_set_io_functions fido_dev_set_transport_functions
	fido_dev_largeblob_get fido_dev_largeblob_set
	fido_dev_largeblob_get fido_dev_largeblob_remove
	fido_dev_largeblob_get fido_dev_largeblob_get_array
	fido_dev_largeblob_get fido_dev_largeblob_set_array
	fido_init fido_set_log_handler
	rs256_pk_new rs256_pk_free
	rs256_pk_new rs256_pk_from_ptr
	rs256_pk_new rs256_pk_from_EVP_PKEY
	rs256_pk_new rs256_pk_from_RSA
	rs256_pk_new rs256_pk_to_EVP_PKEY
)

list(LENGTH MAN_ALIAS MAN_ALIAS_LEN)
math(EXPR MAN_ALIAS_MAX "${MAN_ALIAS_LEN} - 2")

# man_copy
foreach(f ${MAN_SOURCES})
	add_custom_command(OUTPUT ${f}
		COMMAND cp -f ${PROJECT_SOURCE_DIR}/man/${f} .
		DEPENDS ${f})
	list(APPEND COPY_FILES ${f})
endforeach()

# man_lint
foreach(f ${MAN_SOURCES})
	add_custom_command(OUTPUT ${f}.lint
		COMMAND mandoc -T lint -W warning ${f} > ${f}.lint
		DEPENDS ${f})
	list(APPEND LINT_FILES ${f}.lint)
endforeach()

# man_html
foreach(f ${MAN_SOURCES})
	string(REGEX REPLACE "\\.[13]$" "" g ${f})
	add_custom_command(OUTPUT ${g}.html
		COMMAND mandoc -T html -O man="%N.html",style=style.css -I os="Yubico AB" ${f} > ${g}.html
		DEPENDS ${f})
	list(APPEND HTML_FILES ${g}.html)
endforeach()

# man_html_partial
foreach(f ${MAN_SOURCES})
	string(REGEX REPLACE "\\.[13]$" "" g ${f})
	add_custom_command(OUTPUT ${g}.partial
		COMMAND cat ${PROJECT_SOURCE_DIR}/man/dyc.css > ${g}.partial
		COMMAND mandoc -T html -O man="%N.html",fragment ${f} >> ${g}.partial
		DEPENDS ${f})
	list(APPEND HTML_PARTIAL_FILES ${g}.partial)
endforeach()

# man_gzip
foreach(f ${MAN_SOURCES})
	add_custom_command(OUTPUT ${f}.gz
		COMMAND gzip -cn ${f} > ${f}.gz
		DEPENDS ${f})
	list(APPEND GZ_FILES ${f}.gz)
endforeach()

macro(define_symlink_target NAME EXT)
	foreach(i RANGE 0 ${MAN_ALIAS_MAX} 2)
		math(EXPR j "${i} + 1")
		list(GET MAN_ALIAS ${i} SRC)
		list(GET MAN_ALIAS ${j} DST)
		add_custom_command(OUTPUT ${DST}.${EXT}
			COMMAND ln -sf ${SRC}.${EXT} ${DST}.${EXT})
		list(APPEND ${NAME}_LINK_FILES ${DST}.${EXT})
	endforeach()
	add_custom_target(${NAME} DEPENDS ${${NAME}_LINK_FILES})
endmacro()

add_custom_target(man_copy DEPENDS ${COPY_FILES})
add_custom_target(man_lint DEPENDS ${LINT_FILES})
add_custom_target(man_html DEPENDS ${HTML_FILES})
add_custom_target(man_html_partial DEPENDS ${HTML_PARTIAL_FILES})
add_custom_target(man_gzip DEPENDS ${GZ_FILES})

define_symlink_target(man_symlink 3)
define_symlink_target(man_symlink_html html)
define_symlink_target(man_symlink_html_partial partial)
define_symlink_target(man_symlink_gzip 3.gz)

add_dependencies(man_symlink man_copy)
add_dependencies(man_lint man_symlink)
add_dependencies(man_html man_lint)
add_dependencies(man_symlink_html man_html)
add_dependencies(man_html_partial man_lint)
add_dependencies(man_symlink_html_partial man_html_partial)
add_custom_target(man ALL)

if(MANDOC_PATH)
	add_dependencies(man man_symlink_html)
	add_dependencies(man_gzip man_lint)
	install(FILES ${PROJECT_SOURCE_DIR}/man/style.css
		DESTINATION "${CMAKE_INSTALL_DOCDIR}/html")
	foreach(f ${MAN_SOURCES})
		string(REGEX REPLACE "\\.[13]$" "" f ${f})
		install(FILES ${PROJECT_BINARY_DIR}/man/${f}.html
			DESTINATION "${CMAKE_INSTALL_DOCDIR}/html")
	endforeach()
	foreach(i RANGE 0 ${MAN_ALIAS_MAX} 2)
		math(EXPR j "${i} + 1")
		list(GET MAN_ALIAS ${j} DST)
		install(FILES ${PROJECT_BINARY_DIR}/man/${DST}.html
			DESTINATION "${CMAKE_INSTALL_DOCDIR}/html")
	endforeach()
endif()

if(GZIP_PATH)
	add_dependencies(man_gzip man_copy)
	add_dependencies(man_symlink_gzip man_gzip)
	add_dependencies(man man_symlink_gzip)
	foreach(f ${MAN_SOURCES})
		if (${f} MATCHES ".1$")
			install(FILES ${PROJECT_BINARY_DIR}/man/${f}.gz
				DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
		elseif(${f} MATCHES ".3$")
			install(FILES ${PROJECT_BINARY_DIR}/man/${f}.gz
				DESTINATION "${CMAKE_INSTALL_MANDIR}/man3")
		endif()
	endforeach()
	foreach(i RANGE 0 ${MAN_ALIAS_MAX} 2)
		math(EXPR j "${i} + 1")
		list(GET MAN_ALIAS ${j} DST)
		install(FILES ${PROJECT_BINARY_DIR}/man/${DST}.3.gz
			DESTINATION "${CMAKE_INSTALL_MANDIR}/man3")
	endforeach()
else()
	add_dependencies(man man_symlink)
	foreach(f ${MAN_SOURCES})
		if (${f} MATCHES ".1$")
			install(FILES ${PROJECT_BINARY_DIR}/man/${f}
				DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
		elseif(${f} MATCHES ".3$")
			install(FILES ${PROJECT_BINARY_DIR}/man/${f}
				DESTINATION "${CMAKE_INSTALL_MANDIR}/man3")
		endif()
	endforeach()
	foreach(i RANGE 0 ${MAN_ALIAS_MAX} 2)
		math(EXPR j "${i} + 1")
		list(GET MAN_ALIAS ${j} DST)
		install(FILES ${PROJECT_BINARY_DIR}/man/${DST}.3
			DESTINATION "${CMAKE_INSTALL_MANDIR}/man3")
	endforeach()
endif()
