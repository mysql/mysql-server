# ==== Purpose ====
#
# Get the status variable and assert according to the condition.
#

if($func==RESET)
{
        --let $assert_text= 'Value should be 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count", VARIABLE_VALUE, 1] = 0
        --source include/assert.inc

        --let $assert_text= 'Value should be 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_bytes_sum", VARIABLE_VALUE, 1] = 0
        --source include/assert.inc

        --let $assert_text= 'Value should be 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_roundtrip_time_sum", VARIABLE_VALUE, 1] = 0
        --source include/assert.inc
}

if($func==SET)
{
        --let $assert_text= 'Value will be greater than 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count", VARIABLE_VALUE, 1] > 0
        --source include/assert.inc

        --let $assert_text= 'Value will be greater than 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_bytes_sum", VARIABLE_VALUE, 1] > 0
        --source include/assert.inc

        --let $assert_text= 'Value will be greater than 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_roundtrip_time_sum", VARIABLE_VALUE, 1] > 0
        --source include/assert.inc
}

if($func==DATACOUNTEQUAL)
{
        --let $assert_text= 'Gr_data_messages_sent_count value did not change'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count", VARIABLE_VALUE, 1] = [SELECT @prev_count_Gr_data_messages_sent_count]
        --source include/assert.inc

        --let $assert_text= 'Gr_data_messages_sent_bytes_sum value did not change'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_bytes_sum", VARIABLE_VALUE, 1] = [SELECT @prev_count_Gr_data_messages_sent_bytes_sum]
        --source include/assert.inc

        --let $assert_text= 'Gr_data_messages_sent_roundtrip_time_sum value did not change'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_roundtrip_time_sum", VARIABLE_VALUE, 1] = [SELECT @prev_count_Gr_data_messages_sent_roundtrip_time_sum]
        --source include/assert.inc

}
if($func==DATACOUNTGREATER)
{
        --let $assert_text= 'Gr_data_messages_sent_count value increased'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count", VARIABLE_VALUE, 1] > [SELECT @prev_count_Gr_data_messages_sent_count]
        --source include/assert.inc

        --let $assert_text= 'Gr_data_messages_sent_bytes_sum value increased'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_bytes_sum", VARIABLE_VALUE, 1] > [SELECT @prev_count_Gr_data_messages_sent_bytes_sum]
        --source include/assert.inc

        --let $assert_text= 'Gr_data_messages_sent_roundtrip_time_sum value increased'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_roundtrip_time_sum", VARIABLE_VALUE, 1] > [SELECT @prev_count_Gr_data_messages_sent_roundtrip_time_sum]
        --source include/assert.inc

        --let $assert_text= 'Gr_consensus_bytes_sent_sum value increased'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_consensus_bytes_sent_sum", VARIABLE_VALUE, 1] > [SELECT @prev_count_Gr_consensus_bytes_sent_sum]
        --source include/assert.inc

        --let $assert_text= 'Gr_all_consensus_time_sum value increased'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_all_consensus_time_sum", VARIABLE_VALUE, 1] > [SELECT @prev_count_Gr_all_consensus_time_sum]
        --source include/assert.inc

        --let $assert_text= 'Gr_total_messages_sent_count value increased'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_total_messages_sent_count", VARIABLE_VALUE, 1] > [SELECT @prev_count_Gr_total_messages_sent_count]
        --source include/assert.inc
}

if($func==GETPARAM)
{
        SET @prev_count_Gr_data_messages_sent_count=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count");
        SET @prev_count_Gr_data_messages_sent_bytes_sum=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_bytes_sum");
        SET @prev_count_Gr_data_messages_sent_roundtrip_time_sum=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_roundtrip_time_sum");
        SET @prev_count_Gr_consensus_bytes_sent_sum=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_consensus_bytes_sent_sum");
        SET @prev_count_Gr_all_consensus_time_sum=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_all_consensus_time_sum");
        SET @prev_count_Gr_total_messages_sent_count=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_total_messages_sent_count");
}

if($func==GETPREV)
{
        SET @prev_Gr_control_messages_sent_count=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_control_messages_sent_count");
        SET @prev_Gr_control_messages_sent_bytes_sum=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_control_messages_sent_bytes_sum");
        SET @prev_Gr_control_messages_sent_roundtrip_time_sum=(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_control_messages_sent_roundtrip_time_sum");
}

if($func==CONTROLCOUNTGRT)
{
        --let $assert_text= 'Value should be 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_control_messages_sent_count", VARIABLE_VALUE, 1] > [SELECT @prev_Gr_control_messages_sent_count]
        --source include/assert.inc

        --let $assert_text= 'Value should be 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_control_messages_sent_bytes_sum", VARIABLE_VALUE, 1] > [SELECT @prev_Gr_control_messages_sent_bytes_sum]
        --source include/assert.inc

        --let $assert_text= 'Value should be 0'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_control_messages_sent_roundtrip_time_sum", VARIABLE_VALUE, 1] > [SELECT @prev_Gr_control_messages_sent_roundtrip_time_sum]
        --source include/assert.inc
}

#Gr_all_consensus_proposals_count is greater on server1 than on server2
if($func==COMPARECOUNT)
{
        --let $assert_text= 'Value should be greater than other'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_all_consensus_proposals_count", VARIABLE_VALUE, 1] > $Gr_all_consensus_proposals_count_2
        --source include/assert.inc

        --let $assert_text= 'Value should be greater than other'
        --let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_all_consensus_proposals_count", VARIABLE_VALUE, 1] > $Gr_all_consensus_proposals_count_3
        --source include/assert.inc
}