/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysql/gcs/gcs_psi.h"


PSI_thread_key key_GCS_THD_Gcs_ext_logger_impl_m_consumer,
               key_GCS_THD_Gcs_xcom_engine_m_engine_thread,
               key_GCS_THD_Gcs_xcom_control_m_xcom_thread,
               key_GCS_THD_Gcs_xcom_control_m_suspicions_processing_thread;


static PSI_thread_info all_gcs_psi_thread_keys_info[]=
{
  {&key_GCS_THD_Gcs_ext_logger_impl_m_consumer, "THD_Gcs_ext_logger_impl::m_consumer", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GCS_THD_Gcs_xcom_engine_m_engine_thread, "THD_Gcs_xcom_engine::m_engine_thread", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GCS_THD_Gcs_xcom_control_m_xcom_thread, "THD_Gcs_xcom_control::m_xcom_thread", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
  {&key_GCS_THD_Gcs_xcom_control_m_suspicions_processing_thread, "THD_Gcs_xcom_control::m_suspicions_processing_thread", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
};


void register_gcs_thread_psi_keys()
{
  const char *category = "group_rpl";
  int count= static_cast<int>(array_elements(all_gcs_psi_thread_keys_info));

  mysql_thread_register(category, all_gcs_psi_thread_keys_info, count);
}
