select @@collation_connection;
select collation(cast(_latin1 0xDF as char));
select hex(weight_string('s'));
select hex(weight_string(cast(_latin1 0xDF as char)));
select hex(weight_string(cast(_latin1 0xDF as char) as char(1)));
select hex(weight_string('c'));
select hex(weight_string('h'));
select hex(weight_string('ch'));
select hex(weight_string('i'));
# contraction 'ch' plus expansion 'sharp s'
select hex(weight_string(cast(_latin1 0x6368DF as char)));
select hex(weight_string(cast(_latin1 0x6368DF as char) as char(1)));
select hex(weight_string(cast(_latin1 0x6368DF as char) as char(2)));
select hex(weight_string(cast(_latin1 0x6368DF as char) as char(3)));
select hex(weight_string(cast(_latin1 0x6368DF as char) as char(4)));
# expansion 'sharp s' plus contraction 'ch'
select hex(weight_string(cast(_latin1 0xDF6368 as char)));
select hex(weight_string(cast(_latin1 0xDF6368 as char) as char(1)));
select hex(weight_string(cast(_latin1 0xDF6368 as char) as char(2)));
select hex(weight_string(cast(_latin1 0xDF6368 as char) as char(3)));
select hex(weight_string(cast(_latin1 0xDF6368 as char) as char(4)));

# contraction 'ch' plus expansion 'sharp s'
select hex(weight_string(cast(_latin1 0x6368DF as char), 1, 2, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 2, 2, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 3, 2, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 4, 2, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char),25, 2, 0xC0));

select hex(weight_string(cast(_latin1 0x6368DF as char), 1, 3, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 2, 3, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 3, 3, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 4, 3, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char),25, 3, 0xC0));

select hex(weight_string(cast(_latin1 0x6368DF as char), 1, 4, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 2, 4, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 3, 4, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char), 4, 4, 0xC0));
select hex(weight_string(cast(_latin1 0x6368DF as char),25, 4, 0xC0));

# expansion 'sharp s' plus contraction 'ch'
select hex(weight_string(cast(_latin1 0xDF6368 as char), 1, 2,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 2, 2,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 3, 2,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 4, 2,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char),25, 2,0xC0));

select hex(weight_string(cast(_latin1 0xDF6368 as char), 1, 3,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 2, 3,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 3, 3,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 4, 3,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char),25, 3,0xC0));

select hex(weight_string(cast(_latin1 0xDF6368 as char), 1, 4,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 2, 4,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 3, 4,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char), 4, 4,0xC0));
select hex(weight_string(cast(_latin1 0xDF6368 as char),25, 4,0xC0));
