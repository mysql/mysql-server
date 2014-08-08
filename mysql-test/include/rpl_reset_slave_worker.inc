# Auxiliary file used by rpl_reset.inc

if (!$rpl_reset_slave_all)
{
    eval RESET SLAVE $rpl_for_channel_clause;
}

if ($rpl_reset_slave_all)
{
    eval RESET SLAVE ALL $rpl_for_channel_clause;
}

