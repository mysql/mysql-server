# test of group commit switching

query:
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  select | insert | update| delete |
  change_group_commit | change_interval;


select:
	SELECT select_item FROM join where order_by limit;

select_item:
	* | X . _field ;

join:
	_table AS X |
	_table AS X LEFT JOIN _table AS Y ON ( X . _field = Y . _field ) ;

where:
	|
	WHERE X . _field < value |
	WHERE X . _field > value |
	WHERE X . _field = value ;

where_delete:
	|
	WHERE _field < value |
	WHERE _field > value |
	WHERE _field = value ;

order_by:
	| ORDER BY X . _field ;

limit:
	| LIMIT _digit ;

insert:
	INSERT INTO _table ( _field , _field ) VALUES ( value , value ) ;

update:
	UPDATE _table AS X SET _field = value where order_by limit ;

delete:
	DELETE FROM _table where_delete LIMIT _digit ;

value:
	' _letter ' | _digit | _date | _datetime | _time | _english ;

change_group_commit:
        SET GLOBAL MARIA_GROUP_COMMIT=none_soft_hard;

none_soft_hard:
        NONE | SOFT | HARD;

change_interval:
     set_interval | set_interval | set_interval | set_interval |
     drop_interval;

set_interval:
        SET GLOBAL MARIA_GROUP_COMMIT_INTERVAL=_tinyint_unsigned;

drop_interval:
        SET GLOBAL MARIA_GROUP_COMMIT_INTERVAL=0;
