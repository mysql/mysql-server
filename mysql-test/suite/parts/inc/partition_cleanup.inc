################################################################################
# inc/partition_cleanup.inc                                                    #
#                                                                              #
# Purpose:                                                                     #
#   Removal of the objects created by the t/partition_<feature>_<engine>.test  #
#   scripts.                                                                   #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-03-05                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--disable_warnings
DROP VIEW  IF EXISTS v1;
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t0_aux;
DROP TABLE IF EXISTS t0_definition;
DROP TABLE IF EXISTS t0_template;
--enable_warnings
