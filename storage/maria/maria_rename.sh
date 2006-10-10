#!/bin/sh

replace myisam maria MYISAM MARIA MyISAM MARIA -- mysql-test/t/*maria*test mysql-test/r/*maria*result

FILES=`echo sql/ha_maria.{cc,h} include/maria*h storage/maria/*.{c,h}`

replace myisam maria MYISAM MARIA MyISAM MARIA myisam.h maria.h myisamdef.h maria_def.h mi_ maria_ ft_ maria_ft_ "Copyright (C) 2000" "Copyright (C) 2006" MI_ISAMINFO MARIA_INFO MI_CREATE_INFO MARIA_CREATE_INFO maria_isam_ maria_ MI_INFO MARIA_HA MI_ MARIA_ MARIACHK MARIA_CHK  rt_index.h ma_rt_index.h rtree_ maria_rtree rt_key.h ma_rt_key.h rt_mbr.h ma_rt_mbr.h -- $FILES

replace check_table_is_closed _ma_check_table_is_closed test_if_reopen _ma_test_if_reopen my_n_base_info_read maria_n_base_info_read update_auto_increment _ma_update_auto_increment save_pack_length _ma_save_packlength calc_pack_length _ma_calc_pack_length -- $FILES

replace mi_ ma_ ft_ ma_ft_ rt_ ma_rt_ myisam maria myisamchk maria_chk myisampack maria_pack myisamlog maria_log -- storage/maria/Makefile.am

#
# Restore wrong replaces
#

replace maria_sint1korr mi_sint1korr maria_uint1korr mi_uint1korr maria_sint2korr mi_sint2korr maria_sint3korr mi_sint3korr maria_sint4korr mi_sint4korr maria_sint8korr mi_sint8korr maria_uint2korr mi_uint2korr maria_uint3korr mi_uint3korr maria_uint4korr mi_uint4korr maria_uint5korr mi_uint5korr maria_uint6korr mi_uint6korr maria_uint7korr mi_uint7korr maria_uint8korr mi_uint8korr maria_int1store mi_int1store maria_int2store mi_int2store maria_int3store mi_int3store maria_int4store mi_int4store maria_int5store mi_int5store maria_int6store mi_int6store maria_int7store mi_int7store maria_int8store mi_int8store maria_float4store mi_float4store maria_float4get mi_float4get maria_float8store mi_float8store maria_float8get mi_float8get maria_rowstore mi_rowstore maria_rowkorr mi_rowkorr maria_sizestore mi_sizestore maria_sizekorr mi_sizekorr _maria_maria_ _maria MARIA_MAX_POSSIBLE_KEY HA_MAX_POSSIBLE_KEY MARIA_MAX_KEY_BUFF HA_MAX_KEY_BUFF MARIA_MAX_KEY_SEG HA_MAX_KEY_SEG maria_ft_sintXkorr ft_sintXkorr maria_ft_intXstore ft_intXstore maria_ft_boolean_syntax ft_boolean_syntax maria_ft_min_word_len ft_min_word_len maria_ft_max_word_len ft_max_word_len -- $FILES
