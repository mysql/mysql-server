--echo
--echo ===== BEGIN TEST =====

--disable_query_log

# THREAD_ID, EVENT_ID

IF ($key_group == 1)
{
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE event_id != 9999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id ORDER BY thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id ORDER BY thread_id DESC", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND event_id != 99999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND event_id = 99999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id > @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id < @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id <= @target_thread_id", TRUE);
}

# OWNER_THREAD_ID, OWNER_EVENT_ID

IF ($key_group == 1.1)
{
  eval CALL index_test($table, "WHERE owner_thread_id = @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id = @target_thread_id AND owner_event_id = @target_event_id", TRUE);
  eval CALL index_test($table, "WHERE owner_event_id != 9999999", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id = @target_thread_id ORDER BY owner_thread_id", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id = @target_thread_id ORDER BY owner_thread_id DESC", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id = @target_thread_id AND owner_event_id != 99999999", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id = @target_thread_id AND owner_event_id = 99999999", FALSE);
  eval CALL index_test($table, "WHERE owner_thread_id >= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id <= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id = 9999999 OR owner_event_id != 9999999", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id != 9999999 AND owner_event_id = @target_event_id", TRUE);
  eval CALL index_test($table, "WHERE owner_thread_id = 9999999 ORDER BY owner_thread_id", FALSE);
}

# THREAD_ID, ERROR_NUMBER

IF ($key_group == 1.2)
{
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE error_number != 9999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id ORDER BY thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id ORDER BY thread_id DESC", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND error_number != 99999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND error_number = 99999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id > @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id < @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id <= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = 9999999 OR error_number != 9999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id = 9999999 ORDER BY thread_id", FALSE);
  eval CALL index_test($table, "WHERE thread_id > 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id < 9999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id <= 9999999", TRUE);
}

# USER, HOST, EVENT_NAME

IF ($key_group == 2)
{
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1'", TRUE);
  #eval CALL index_test($table, "WHERE host != '$target_host' AND user != 'user1'", FALSE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1' AND event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1' AND event_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND user= 'nouser' AND event_name = '$target_event_name'", FALSE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'nouser' AND event_name = '$target_event_name'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND user= 'user1' AND event_name = '$target_event_name'", FALSE);
}

# USER, HOST

IF ($key_group == 2.1)
{
  eval CALL index_test($table, "WHERE host = '$target_host'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1'", TRUE);
  #eval CALL index_test($table, "WHERE host != '$target_host' AND user != 'user1'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND user= 'nouser'", FALSE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'nouser'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND user= 'user1'", FALSE);
}

# PROCESSLIST_USER, PROCESSLIST_HOST

IF ($key_group == 2.1.1)
{
  eval CALL index_test($table, "WHERE processlist_host = '$target_host'", TRUE);
  eval CALL index_test($table, "WHERE processlist_host = '$target_host' AND processlist_user= '$target_user'", TRUE);
  eval CALL index_test($table, "WHERE processlist_host != '$target_host' AND processlist_user != '$target_user'", FALSE);
  eval CALL index_test($table, "WHERE processlist_host = 'nohost' AND processlist_user= 'nouser'", FALSE);
  eval CALL index_test($table, "WHERE processlist_host = '$target_host' AND processlist_user= 'nouser'", FALSE);
  eval CALL index_test($table, "WHERE processlist_host = 'nohost' AND processlist_user= '$target_user'", FALSE);
}

# USER, HOST, VARIABLE_NAME

IF ($key_group == 2.2)
{
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1' AND variable_name = '$target_variable_name'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1' AND variable_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND user= 'nouser' AND variable_name = '$target_variable_name'", FALSE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'nouser' AND variable_name = '$target_variable_name'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND user= 'user1' AND variable_name = '$target_variable_name'", FALSE);
}

# USER, HOST, ERROR_NUMBER

IF ($key_group == 2.3)
{
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1' AND error_number = '$target_error_number'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'user1' AND error_number = 999999", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND user= 'nouser' AND error_number = '$target_error_number'", FALSE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND user= 'nouser' AND error_number = '$target_error_number'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND user= 'user1' AND error_number = '$target_error_number'", FALSE);
}

# HOST, ERROR_NUMBER

IF ($key_group == 2.4)
{
  eval CALL index_test($table, "WHERE host = '$target_host'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND error_number = '$target_error_number'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND error_number = 999999", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND error_number = '$target_error_number'", FALSE);
}

# USER, ERROR_NUMBER

IF ($key_group == 2.5)
{
  eval CALL index_test($table, "WHERE user= 'user1'", TRUE);
  eval CALL index_test($table, "WHERE user= 'user1' AND error_number = '$target_error_number'", TRUE);
  eval CALL index_test($table, "WHERE user= 'user1' AND error_number = 999999", FALSE);
  eval CALL index_test($table, "WHERE user= 'nouser' AND error_number = '$target_error_number'", FALSE);
}

# ERROR_NUMBER

IF ($key_group == 2.6)
{
  eval CALL index_test($table, "WHERE error_number = '$target_error_number'", TRUE);
  eval CALL index_test($table, "WHERE error_number = '$target_error_number' ORDER BY error_number", TRUE);
  eval CALL index_test($table, "WHERE error_number = '$target_error_number' ORDER BY error_number DESC", TRUE);
  eval CALL index_test($table, "WHERE error_number > '$target_error_number'", TRUE);
  eval CALL index_test($table, "WHERE error_number < '$target_error_number'", TRUE);
  eval CALL index_test($table, "WHERE error_number >= '$target_error_number'", TRUE);
  eval CALL index_test($table, "WHERE error_number <= '$target_error_number'", TRUE);
  eval CALL index_test($table, "WHERE error_number = 9999999 ORDER BY error_number", FALSE);
  eval CALL index_test($table, "WHERE error_number > 9999999", FALSE);
  eval CALL index_test($table, "WHERE error_number < 9999999", TRUE);
  eval CALL index_test($table, "WHERE error_number >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE error_number <= 9999999", TRUE);
}

# USER, HOST, ROLE

IF ($key_group == 2.7)
{
   eval CALL index_test($table, "WHERE host = '$target_host' AND user = 'user1'", TRUE);
   eval CALL index_test($table, "WHERE host = '$target_host' AND user = 'user1' AND `role` = '%'", TRUE);
   eval CALL index_test($table, "WHERE host = '$target_host' AND user = 'user1' AND `role` = 'impossible'", FALSE);
   eval CALL index_test($table, "WHERE host = 'nohost' AND user = 'nouser' AND `role` = '%'", FALSE);
   eval CALL index_test($table, "WHERE host = '$target_host' AND user = 'nouser' AND `role` = '%'", FALSE);
   eval CALL index_test($table, "WHERE host = 'nohost' AND user = 'user1' AND `role` = '%'", FALSE);
}

# USER, EVENT_NAME

IF ($key_group == 3)
{
  eval CALL index_test($table, "WHERE user= 'user1'", TRUE);
  eval CALL index_test($table, "WHERE user= 'user1' AND event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE user= 'user1' AND event_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE user= 'nouser' AND event_name = '$target_event_name'", FALSE);
}

# USER, VARIABLE_NAME

IF ($key_group == 3.1)
{
  eval CALL index_test($table, "WHERE user= 'user1'", TRUE);
  eval CALL index_test($table, "WHERE user= 'user1' AND variable_name = '$target_variable_name'", TRUE);
  eval CALL index_test($table, "WHERE user= 'user1' AND variable_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE user= 'nouser' AND variable_name = '$target_variable_name'", FALSE);
}

# THREAD_ID, EVENT_NAME

IF ($key_group == 4)
{
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE event_name != '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id ORDER BY thread_id DESC", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND event_name != '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND event_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE thread_id > @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id < @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id <= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = 9999999 OR event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id > 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id < 9999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id <= 9999999", TRUE);
}

IF ($key_group == 4.1)
{
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id ORDER BY thread_id DESC", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND event_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE thread_id > @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id < @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id <= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = 9999999 OR event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id > 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id < 9999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id <= 9999999", TRUE);
}

# EVENT_NAME

IF ($key_group == 5)
{
  eval CALL index_test($table, "WHERE event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE event_name != '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE event_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE event_name != 'impossible' ORDER BY event_name DESC", TRUE);
}

# EVENT_NAME (for events with only 1 class)

IF ($key_group == 5.1)
{
  eval CALL index_test($table, "WHERE event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE event_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE event_name != 'impossible' ORDER BY event_name DESC", TRUE);
}

# HOST, EVENT_NAME

IF ($key_group == 6)
{
  eval CALL index_test($table, "WHERE host = '$target_host'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND event_name = '$target_event_name'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND event_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND event_name = '$target_event_name'", FALSE);
}

# HOST, VARIABLE_NAME

IF ($key_group == 6.1)
{
  eval CALL index_test($table, "WHERE host = '$target_host'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND variable_name = '$target_variable_name'", TRUE);
  eval CALL index_test($table, "WHERE host = '$target_host' AND variable_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE host = 'nohost' AND variable_name = '$target_variable_name'", FALSE);
}

# HOST

IF ($key_group == 6.2)
{
  eval CALL index_test($table, "WHERE host = '$target_host'", TRUE);
  eval CALL index_test($table, "WHERE host IS NULL", TRUE);
  eval CALL index_test($table, "WHERE host IS NOT NULL", TRUE);
  eval CALL index_test($table, "WHERE host = 'nohost'", FALSE);
}

# PROCESSLIST_HOST

IF ($key_group == 6.3)
{
  eval CALL index_test($table, "WHERE processlist_host = '$target_host'", TRUE);
  eval CALL index_test($table, "WHERE processlist_host IS NULL", TRUE);
  eval CALL index_test($table, "WHERE processlist_host IS NOT NULL", TRUE);
  eval CALL index_test($table, "WHERE processlist_host = 'nohost'", FALSE);
}

# THREAD_ID, VARIABLE_NAME

IF ($key_group == 7)
{
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id ORDER BY thread_id DESC", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND variable_name = '$target_variable_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND variable_name != '$target_variable_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND variable_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE thread_id > @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id < @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id <= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = 9999999 OR variable_name = '$target_variable_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id > 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id < 9999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id <= 9999999", TRUE);
}

# VARIABLE_NAME

IF ($key_group == 8)
{
  eval CALL index_test($table, "WHERE variable_name = '$target_variable_name'", TRUE);
  eval CALL index_test($table, "WHERE variable_name != '$target_variable_name'", TRUE);
  eval CALL index_test($table, "WHERE variable_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE variable_name != 'impossible' ORDER BY variable_name DESC", TRUE);
}

# THREAD_ID, NAME

IF ($key_group == 9)
{
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id ORDER BY thread_id DESC", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND name != '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id = @target_thread_id AND name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE thread_id > @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id < @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id <= @target_thread_id", TRUE);
  eval CALL index_test($table, "WHERE thread_id = 9999999 OR name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE thread_id > 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id < 9999999", TRUE);
  eval CALL index_test($table, "WHERE thread_id >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE thread_id <= 9999999", TRUE);
}

# PROCESSLIST_ID, ATTR_NAME -- Multiple connections

IF ($key_group == 9.1)
{
  eval CALL index_test($table, "WHERE processlist_id = @target_id", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = @target_id ORDER BY processlist_id DESC", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = @target_id AND attr_name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = @target_id AND attr_name != '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = @target_id AND attr_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE processlist_id > @target_id", TRUE);
  eval CALL index_test($table, "WHERE processlist_id < @target_id", TRUE);
  eval CALL index_test($table, "WHERE processlist_id >= @target_id", TRUE);
  eval CALL index_test($table, "WHERE processlist_id <= @target_id", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = 9999999 OR attr_name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE processlist_id > 9999999", FALSE);
  eval CALL index_test($table, "WHERE processlist_id < 9999999", TRUE);
  eval CALL index_test($table, "WHERE processlist_id >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE processlist_id <= 9999999", TRUE);
}

# PROCESSLIST_ID, ATTR_NAME -- Current connection

IF ($key_group == 9.2)
{
  eval CALL index_test($table, "WHERE processlist_id = @target_id", TRUE);
  eval CALL index_test($table, "WHERE attr_name != '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = @target_id ORDER BY processlist_id DESC", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = @target_id AND attr_name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = @target_id AND attr_name != '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = @target_id AND attr_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE processlist_id > @target_id", FALSE);
  eval CALL index_test($table, "WHERE processlist_id < @target_id", FALSE);
  eval CALL index_test($table, "WHERE processlist_id >= @target_id", TRUE);
  eval CALL index_test($table, "WHERE processlist_id <= @target_id", TRUE);
  eval CALL index_test($table, "WHERE processlist_id = 9999999 OR attr_name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE processlist_id > 9999999", FALSE);
  eval CALL index_test($table, "WHERE processlist_id < 9999999", TRUE);
  eval CALL index_test($table, "WHERE processlist_id >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE processlist_id <= 9999999", TRUE);
}

# OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME

IF ($key_group == 10)
{
  eval CALL index_test($table, "WHERE object_type = '$target_type'", TRUE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = '$target_schema'", TRUE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = '$target_schema' AND object_name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE object_type = 'impossible' AND object_schema = '$target_schema' AND object_name = '$target_name'", FALSE);
  eval CALL index_test($table, "WHERE object_type = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = '$target_schema' AND object_name = 'impossible'", FALSE);
}

# OWNER_OBJECT_TYPE, OWNER_OBJECT_SCHEMA, OWNER_OBJECT_NAME

IF ($key_group == 10.1)
{
  eval CALL index_test($table, "WHERE owner_object_type = '$target_type'", TRUE);
  eval CALL index_test($table, "WHERE owner_object_type = '$target_type' AND owner_object_schema = '$target_schema'", TRUE);
  eval CALL index_test($table, "WHERE owner_object_type = '$target_type' AND owner_object_schema = '$target_schema' AND owner_object_name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE owner_object_type = 'impossible' AND owner_object_schema = '$target_schema' AND owner_object_name = '$target_name'", FALSE);
  eval CALL index_test($table, "WHERE owner_object_type = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE owner_object_type = '$target_type' AND owner_object_schema = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE owner_object_type = '$target_type' AND owner_object_schema = '$target_schema' AND owner_object_name = 'impossible'", FALSE);
}

# OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME

IF ($key_group == 11)
{
  eval CALL index_test($table, "WHERE object_type = '$target_type'", TRUE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = '$target_schema'", TRUE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = '$target_schema' AND object_name = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = '$target_schema' AND object_name = '$target_name' AND index_name = '$target_index'", TRUE);
  eval CALL index_test($table, "WHERE object_type = 'impossible' AND object_schema = '$target_schema' AND object_name = '$target_name'", FALSE);
  eval CALL index_test($table, "WHERE object_type = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = '$target_schema' AND object_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE object_type = '$target_type' AND object_schema = '$target_schema' AND object_name = '$target_name' AND index_name = 'impossible'", FALSE);
}

# OBJECT_INSTANCE_BEGIN

IF ($key_group == 12)
{
  eval CALL index_test($table, "WHERE object_instance_begin = @object_instance_begin", TRUE);

  # NOTE: The following test is disabled for metadata locks because it will give
  # a different number of locks for indexed and non-indexed access. The
  # non-indexed access will try to use histogram statistics, and will thus use
  # one more metadata lock.
  IF ($table != "metadata_locks")
  {
    eval CALL index_test($table, "WHERE object_instance_begin != @object_instance_begin", TRUE);
  }

  eval CALL index_test($table, "WHERE object_instance_begin = '9999999999'", FALSE);
}

# GENERIC NAME - EVENT_NAME / NAME / STATEMENT_NAME

IF ($key_group == 13)
{
  eval CALL index_test($table, "WHERE $name_column = '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE $name_column != '$target_name'", TRUE);
  eval CALL index_test($table, "WHERE $name_column = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE $name_column != 'impossible' ORDER BY $name_column DESC", TRUE);
}

# FILE_NAME

IF ($key_group == 13.1)
{
  eval CALL index_test($table, "WHERE $name_column = @target_name", TRUE);
  eval CALL index_test($table, "WHERE $name_column != @target_name", TRUE);
  eval CALL index_test($table, "WHERE $name_column = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE $name_column != 'impossible' ORDER BY $name_column DESC", TRUE);
}

# GENERIC ID - THREAD_ID / LOCKED_BY_THREAD_ID / STATEMENT_ID / SOCKET_ID

IF ($key_group == 14)
{
  eval CALL index_test($table, "WHERE $id_column = @target_id", TRUE);
  eval CALL index_test($table, "WHERE $id_column = @target_id ORDER BY $id_column", TRUE);
  eval CALL index_test($table, "WHERE $id_column = @target_id ORDER BY $id_column DESC", TRUE);
  eval CALL index_test($table, "WHERE $id_column = 9999999 ORDER BY $id_column", FALSE);
  eval CALL index_test($table, "WHERE $id_column IS NOT NULL", TRUE);
}

# IP

IF ($key_group == 15)
{
  eval CALL index_test($table, "WHERE IP = '$target_ip'", TRUE);
  #eval CALL index_test($table, "WHERE IP != '$target_ip'", FALSE);
  eval CALL index_test($table, "WHERE IP = 'impossible'", FALSE);
}

# PORT

IF ($key_group == 16)
{
  eval CALL index_test($table, "WHERE port = @target_port", TRUE);
  eval CALL index_test($table, "WHERE port = @target_port ORDER BY port", TRUE);
  eval CALL index_test($table, "WHERE port = 9999999", FALSE);
  eval CALL index_test($table, "WHERE port > 9999999", FALSE);
  eval CALL index_test($table, "WHERE port < 9999999", TRUE);
  eval CALL index_test($table, "WHERE port >= 9999999", FALSE);
  eval CALL index_test($table, "WHERE port <= 9999999", TRUE);
}

# IP, PORT

IF ($key_group == 16.1)
{
  eval CALL index_test($table, "WHERE IP = '$target_ip'", TRUE);
  eval CALL index_test($table, "WHERE IP = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE IP = '$target_ip' AND port = @target_port", TRUE);
  eval CALL index_test($table, "WHERE IP = '$target_ip' AND port = 9999999", FALSE);
  eval CALL index_test($table, "WHERE IP = 'impossible' AND port = @target_port", FALSE);
  eval CALL index_test($table, "WHERE port = @target_port", TRUE);
}

# SCHEMA_NAME, DIGEST

IF ($key_group == 17)
{
  eval CALL index_test($table, "WHERE schema_name = '$target_schema'", TRUE);
  eval CALL index_test($table, "WHERE schema_name != '$target_schema'", TRUE);
  eval CALL index_test($table, "WHERE schema_name = '$target_schema' AND digest = @target_digest", TRUE);
  eval CALL index_test($table, "WHERE schema_name = '$target_schema' AND digest = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE schema_name = 'noschema_name' AND digest = @target_digest", FALSE);
}

# CHANNEL_NAME

IF ($key_group == 18)
{
  eval CALL index_test($table, "WHERE channel_name = '$target_channel_name'", TRUE);
  #eval CALL index_test($table, "WHERE channel_name != '$target_channel_name'", TRUE);
  eval CALL index_test($table, "WHERE channel_name = 'impossible'", FALSE);
  eval CALL index_test($table, "WHERE channel_name != 'impossible' ORDER BY channel_name DESC", TRUE);
}



--enable_query_log
