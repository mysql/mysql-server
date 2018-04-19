

--disable_warnings
drop table if exists t1_30237_bool;
--enable_warnings

create table t1_30237_bool(A boolean, B boolean, C boolean);

insert into t1_30237_bool values
(FALSE, FALSE, FALSE),
(FALSE, FALSE, NULL),
(FALSE, FALSE, TRUE),
(FALSE, NULL, FALSE),
(FALSE, NULL, NULL),
(FALSE, NULL, TRUE),
(FALSE, TRUE, FALSE),
(FALSE, TRUE, NULL),
(FALSE, TRUE, TRUE),
(NULL, FALSE, FALSE),
(NULL, FALSE, NULL),
(NULL, FALSE, TRUE),
(NULL, NULL, FALSE),
(NULL, NULL, NULL),
(NULL, NULL, TRUE),
(NULL, TRUE, FALSE),
(NULL, TRUE, NULL),
(NULL, TRUE, TRUE),
(TRUE, FALSE, FALSE),
(TRUE, FALSE, NULL),
(TRUE, FALSE, TRUE),
(TRUE, NULL, FALSE),
(TRUE, NULL, NULL),
(TRUE, NULL, TRUE),
(TRUE, TRUE, FALSE),
(TRUE, TRUE, NULL),
(TRUE, TRUE, TRUE) ;

--echo Testing OR, XOR, AND
select A, B, A OR B, A XOR B, A AND B
  from t1_30237_bool where C is null order by A, B;

--echo Testing that OR is associative 
select A, B, C, (A OR B) OR C, A OR (B OR C), A OR B OR C
 from t1_30237_bool order by A, B, C;

select count(*) from t1_30237_bool
  where ((A OR B) OR C) != (A OR (B OR C));

--echo Testing that XOR is associative 
select A, B, C, (A XOR B) XOR C, A XOR (B XOR C), A XOR B XOR C
  from t1_30237_bool order by A, B, C;

select count(*) from t1_30237_bool
  where ((A XOR B) XOR C) != (A XOR (B XOR C));

--echo Testing that AND is associative 
select A, B, C, (A AND B) AND C, A AND (B AND C), A AND B AND C
  from t1_30237_bool order by A, B, C;

select count(*) from t1_30237_bool
  where ((A AND B) AND C) != (A AND (B AND C));

--echo Testing that AND has precedence over OR
select A, B, C, (A OR B) AND C, A OR (B AND C), A OR B AND C
  from t1_30237_bool order by A, B, C;
select count(*) from t1_30237_bool
  where (A OR (B AND C)) != (A OR B AND C);
select A, B, C, (A AND B) OR C, A AND (B OR C), A AND B OR C
  from t1_30237_bool order by A, B, C;
select count(*) from t1_30237_bool
  where ((A AND B) OR C) != (A AND B OR C);

--echo Testing that AND has precedence over XOR
select A, B, C, (A XOR B) AND C, A XOR (B AND C), A XOR B AND C
  from t1_30237_bool order by A, B, C;
select count(*) from t1_30237_bool
  where (A XOR (B AND C)) != (A XOR B AND C);
select A, B, C, (A AND B) XOR C, A AND (B XOR C), A AND B XOR C
  from t1_30237_bool order by A, B, C;
select count(*) from t1_30237_bool
  where ((A AND B) XOR C) != (A AND B XOR C);

--echo Testing that XOR has precedence over OR
select A, B, C, (A XOR B) OR C, A XOR (B OR C), A XOR B OR C
  from t1_30237_bool order by A, B, C;
select count(*) from t1_30237_bool
  where ((A XOR B) OR C) != (A XOR B OR C);
select A, B, C, (A OR B) XOR C, A OR (B XOR C), A OR B XOR C
  from t1_30237_bool order by A, B, C;
select count(*) from t1_30237_bool
  where (A OR (B XOR C)) != (A OR B XOR C);

drop table t1_30237_bool;

--echo Testing that NOT has precedence over OR
select (NOT FALSE) OR TRUE, NOT (FALSE OR TRUE), NOT FALSE OR TRUE;

--echo Testing that NOT has precedence over XOR
select (NOT FALSE) XOR FALSE, NOT (FALSE XOR FALSE), NOT FALSE XOR FALSE;

--echo Testing that NOT has precedence over AND
select (NOT FALSE) AND FALSE, NOT (FALSE AND FALSE), NOT FALSE AND FALSE;

--echo Testing that NOT is associative
select NOT NOT TRUE, NOT NOT NOT FALSE;

--echo Testing that IS has precedence over NOT
select (NOT NULL) IS TRUE, NOT (NULL IS TRUE), NOT NULL IS TRUE;
select (NOT NULL) IS NOT TRUE, NOT (NULL IS NOT TRUE), NOT NULL IS NOT TRUE;
select (NOT NULL) IS FALSE, NOT (NULL IS FALSE), NOT NULL IS FALSE;
select (NOT NULL) IS NOT FALSE, NOT (NULL IS NOT FALSE), NOT NULL IS NOT FALSE;
select (NOT TRUE) IS UNKNOWN, NOT (TRUE IS UNKNOWN), NOT TRUE IS UNKNOWN;
select (NOT TRUE) IS NOT UNKNOWN, NOT (TRUE IS NOT UNKNOWN), NOT TRUE IS NOT UNKNOWN;
select (NOT TRUE) IS NULL, NOT (TRUE IS NULL), NOT TRUE IS NULL;
select (NOT TRUE) IS NOT NULL, NOT (TRUE IS NOT NULL), NOT TRUE IS NOT NULL;

--echo Testing that IS [NOT] TRUE/FALSE/UNKNOWN predicates are not associative
# Documenting existing behavior in 5.0.48
-- error ER_PARSE_ERROR
select TRUE IS TRUE IS TRUE IS TRUE;
-- error ER_PARSE_ERROR
select FALSE IS NOT TRUE IS NOT TRUE IS NOT TRUE;
-- error ER_PARSE_ERROR
select NULL IS FALSE IS FALSE IS FALSE;
-- error ER_PARSE_ERROR
select TRUE IS NOT FALSE IS NOT FALSE IS NOT FALSE;
-- error ER_PARSE_ERROR
select FALSE IS UNKNOWN IS UNKNOWN IS UNKNOWN;
-- error ER_PARSE_ERROR
select TRUE IS NOT UNKNOWN IS NOT UNKNOWN IS NOT UNKNOWN;

--echo Testing that IS [NOT] NULL predicates are associative
# Documenting existing behavior in 5.0.48
select FALSE IS NULL IS NULL IS NULL;
select TRUE IS NOT NULL IS NOT NULL IS NOT NULL;

--echo Testing that comparison operators are left associative
select 1 <=> 2 <=> 2, (1 <=> 2) <=> 2, 1 <=> (2 <=> 2);
select 1 = 2 = 2, (1 = 2) = 2, 1 = (2 = 2);
select 1 != 2 != 3, (1 != 2) != 3, 1 != (2 != 3);
select 1 <> 2 <> 3, (1 <> 2) <> 3, 1 <> (2 <> 3);
select 1 < 2 < 3, (1 < 2) < 3, 1 < (2 < 3);
select 3 <= 2 <= 1, (3 <= 2) <= 1, 3 <= (2 <= 1);
select 1 > 2 > 3, (1 > 2) > 3, 1 > (2 > 3);
select 1 >= 2 >= 3, (1 >= 2) >= 3, 1 >= (2 >= 3);

-- echo Testing that | is associative
select 0xF0 | 0x0F | 0x55, (0xF0 | 0x0F) | 0x55, 0xF0 | (0x0F | 0x55);

-- echo Testing that & is associative
select 0xF5 & 0x5F & 0x55, (0xF5 & 0x5F) & 0x55, 0xF5 & (0x5F & 0x55);

-- echo Testing that << is left associative
select 4 << 3 << 2, (4 << 3) << 2, 4 << (3 << 2);

-- echo Testing that >> is left associative
select 256 >> 3 >> 2, (256 >> 3) >> 2, 256 >> (3 >> 2);

--echo Testing that & has precedence over |
select 0xF0 & 0x0F | 0x55, (0xF0 & 0x0F) | 0x55, 0xF0 & (0x0F | 0x55);
select 0x55 | 0xF0 & 0x0F, (0x55 | 0xF0) & 0x0F, 0x55 | (0xF0 & 0x0F);

--echo Testing that << has precedence over |
select 0x0F << 4 | 0x0F, (0x0F << 4) | 0x0F, 0x0F << (4 | 0x0F);
select 0x0F | 0x0F << 4, (0x0F | 0x0F) << 4, 0x0F | (0x0F << 4);

--echo Testing that >> has precedence over |
select 0xF0 >> 4 | 0xFF, (0xF0 >> 4) | 0xFF, 0xF0 >> (4 | 0xFF);
select 0xFF | 0xF0 >> 4, (0xFF | 0xF0) >> 4, 0xFF | (0xF0 >> 4);

--echo Testing that << has precedence over &
select 0x0F << 4 & 0xF0, (0x0F << 4) & 0xF0, 0x0F << (4 & 0xF0);
select 0xF0 & 0x0F << 4, (0xF0 & 0x0F) << 4, 0xF0 & (0x0F << 4);

--echo Testing that >> has precedence over &
select 0xF0 >> 4 & 0x55, (0xF0 >> 4) & 0x55, 0xF0 >> (4 & 0x55);
select 0x0F & 0xF0 >> 4, (0x0F & 0xF0) >> 4, 0x0F & (0xF0 >> 4);

--echo Testing that >> and << have the same precedence
select 0xFF >> 4 << 2, (0xFF >> 4) << 2, 0xFF >> (4 << 2);
select 0x0F << 4 >> 2, (0x0F << 4) >> 2, 0x0F << (4 >> 2);

--echo Testing that binary + is associative
select 1 + 2 + 3, (1 + 2) + 3, 1 + (2 + 3);

--echo Testing that binary - is left associative
select 1 - 2 - 3, (1 - 2) - 3, 1 - (2 - 3);

--echo Testing that binary + and binary - have the same precedence
# evaluated left to right
select 1 + 2 - 3, (1 + 2) - 3, 1 + (2 - 3);
select 1 - 2 + 3, (1 - 2) + 3, 1 - (2 + 3);

--echo Testing that binary + has precedence over |
select 0xF0 + 0x0F | 0x55, (0xF0 + 0x0F) | 0x55, 0xF0 + (0x0F | 0x55);
select 0x55 | 0xF0 + 0x0F, (0x55 | 0xF0) + 0x0F, 0x55 | (0xF0 + 0x0F);

--echo Testing that binary + has precedence over &
select 0xF0 + 0x0F & 0x55, (0xF0 + 0x0F) & 0x55, 0xF0 + (0x0F & 0x55);
select 0x55 & 0xF0 + 0x0F, (0x55 & 0xF0) + 0x0F, 0x55 & (0xF0 + 0x0F);

--echo Testing that binary + has precedence over <<
select 2 + 3 << 4, (2 + 3) << 4, 2 + (3 << 4);
select 3 << 4 + 2, (3 << 4) + 2, 3 << (4 + 2);

--echo Testing that binary + has precedence over >>
select 4 + 3 >> 2, (4 + 3) >> 2, 4 + (3 >> 2);
select 3 >> 2 + 1, (3 >> 2) + 1, 3 >> (2 + 1);

--echo Testing that binary - has precedence over |
select 0xFF - 0x0F | 0x55, (0xFF - 0x0F) | 0x55, 0xFF - (0x0F | 0x55);
select 0x55 | 0xFF - 0xF0, (0x55 | 0xFF) - 0xF0, 0x55 | (0xFF - 0xF0);

--echo Testing that binary - has precedence over &
select 0xFF - 0xF0 & 0x55, (0xFF - 0xF0) & 0x55, 0xFF - (0xF0 & 0x55);
select 0x55 & 0xFF - 0x49, (0x55 & 0xFF) - 0x49, 0x55 & (0xFF - 0x49);

--echo Testing that binary - has precedence over <<
select 16 - 3 << 2, (16 - 3) << 2, 16 - (3 << 2);
select 4 << 3 - 2, (4 << 3) - 2, 4 << (3 - 2);

--echo Testing that binary - has precedence over >>
select 16 - 3 >> 2, (16 - 3) >> 2, 16 - (3 >> 2);
select 16 >> 3 - 2, (16 >> 3) - 2, 16 >> (3 - 2);

--echo Testing that * is associative
select 2 * 3 * 4, (2 * 3) * 4, 2 * (3 * 4);

--echo Testing that * has precedence over |
select 2 * 0x40 | 0x0F, (2 * 0x40) | 0x0F, 2 * (0x40 | 0x0F);
select 0x0F | 2 * 0x40, (0x0F | 2) * 0x40, 0x0F | (2 * 0x40);

--echo Testing that * has precedence over &
select 2 * 0x40 & 0x55, (2 * 0x40) & 0x55, 2 * (0x40 & 0x55);
select 0xF0 & 2 * 0x40, (0xF0 & 2) * 0x40, 0xF0 & (2 * 0x40);

--echo Testing that * has precedence over << 
# Actually, can't prove it for the first case,
# since << is a multiplication by a power of 2,
# and * is associative
select 5 * 3 << 4, (5 * 3) << 4, 5 * (3 << 4);
select 2 << 3 * 4, (2 << 3) * 4, 2 << (3 * 4);

--echo Testing that * has precedence over >>
# >> is a multiplication by a (negative) power of 2,
# see above.
select 3 * 4 >> 2, (3 * 4) >> 2, 3 * (4 >> 2);
select 4 >> 2 * 3, (4 >> 2) * 3, 4 >> (2 * 3);

--echo Testing that * has precedence over binary +
select 2 * 3 + 4, (2 * 3) + 4, 2 * (3 + 4);
select 2 + 3 * 4, (2 + 3) * 4, 2 + (3 * 4);

--echo Testing that * has precedence over binary -
select 4 * 3 - 2, (4 * 3) - 2, 4 * (3 - 2);
select 4 - 3 * 2, (4 - 3) * 2, 4 - (3 * 2);

--echo Testing that / is left associative
select 15 / 5 / 3, (15 / 5) / 3, 15 / (5 / 3);

--echo Testing that / has precedence over |
select 105 / 5 | 2, (105 / 5) | 2, 105 / (5 | 2);
select 105 | 2 / 5, (105 | 2) / 5, 105 | (2 / 5);

--echo Testing that / has precedence over &
select 105 / 5 & 0x0F, (105 / 5) & 0x0F, 105 / (5 & 0x0F);
select 0x0F & 105 / 5, (0x0F & 105) / 5, 0x0F & (105 / 5);

--echo Testing that / has precedence over << 
select 0x80 / 4 << 2, (0x80 / 4) << 2, 0x80 / (4 << 2);
select 0x80 << 4 / 2, (0x80 << 4) / 2, 0x80 << (4 / 2);

--echo Testing that / has precedence over >>
select 0x80 / 4 >> 2, (0x80 / 4) >> 2, 0x80 / (4 >> 2);
select 0x80 >> 4 / 2, (0x80 >> 4) / 2, 0x80 >> (4 / 2);

--echo Testing that / has precedence over binary +
select 0x80 / 2 + 2, (0x80 / 2) + 2, 0x80 / (2 + 2);
select 0x80 + 2 / 2, (0x80 + 2) / 2, 0x80 + (2 / 2);

--echo Testing that / has precedence over binary -
select 0x80 / 4 - 2, (0x80 / 4) - 2, 0x80 / (4 - 2);
select 0x80 - 4 / 2, (0x80 - 4) / 2, 0x80 - (4 / 2);

# TODO: %, DIV, MOD

--echo Testing that ^ is associative
select 0xFF ^ 0xF0 ^ 0x0F, (0xFF ^ 0xF0) ^ 0x0F, 0xFF ^ (0xF0 ^ 0x0F);
select 0xFF ^ 0xF0 ^ 0x55, (0xFF ^ 0xF0) ^ 0x55, 0xFF ^ (0xF0 ^ 0x55);

--echo Testing that ^ has precedence over |
select 0xFF ^ 0xF0 | 0x0F, (0xFF ^ 0xF0) | 0x0F, 0xFF ^ (0xF0 | 0x0F);
select 0xF0 | 0xFF ^ 0xF0, (0xF0 | 0xFF) ^ 0xF0, 0xF0 | (0xFF ^ 0xF0);

--echo Testing that ^ has precedence over &
select 0xFF ^ 0xF0 & 0x0F, (0xFF ^ 0xF0) & 0x0F, 0xFF ^ (0xF0 & 0x0F);
select 0x0F & 0xFF ^ 0xF0, (0x0F & 0xFF) ^ 0xF0, 0x0F & (0xFF ^ 0xF0);

--echo Testing that ^ has precedence over <<
select 0xFF ^ 0xF0 << 2, (0xFF ^ 0xF0) << 2, 0xFF ^ (0xF0 << 2);
select 0x0F << 2 ^ 0xFF, (0x0F << 2) ^ 0xFF, 0x0F << (2 ^ 0xFF);

--echo Testing that ^ has precedence over >>
select 0xFF ^ 0xF0 >> 2, (0xFF ^ 0xF0) >> 2, 0xFF ^ (0xF0 >> 2);
select 0xFF >> 2 ^ 0xF0, (0xFF >> 2) ^ 0xF0, 0xFF >> (2 ^ 0xF0);

--echo Testing that ^ has precedence over binary +
select 0xFF ^ 0xF0 + 0x0F, (0xFF ^ 0xF0) + 0x0F, 0xFF ^ (0xF0 + 0x0F);
select 0x0F + 0xFF ^ 0xF0, (0x0F + 0xFF) ^ 0xF0, 0x0F + (0xFF ^ 0xF0);

--echo Testing that ^ has precedence over binary -
select 0xFF ^ 0xF0 - 1, (0xFF ^ 0xF0) - 1, 0xFF ^ (0xF0 - 1);
select 0x65 - 0x0F ^ 0x55, (0x65 - 0x0F) ^ 0x55, 0x65 - (0x0F ^ 0x55);

--echo Testing that ^ has precedence over *
select 0xFF ^ 0xF0 * 2, (0xFF ^ 0xF0) * 2, 0xFF ^ (0xF0 * 2);
select 2 * 0xFF ^ 0xF0, (2 * 0xFF) ^ 0xF0, 2 * (0xFF ^ 0xF0);

--echo Testing that ^ has precedence over /
select 0xFF ^ 0xF0 / 2, (0xFF ^ 0xF0) / 2, 0xFF ^ (0xF0 / 2);
select 0xF2 / 2 ^ 0xF0, (0xF2 / 2) ^ 0xF0, 0xF2 / (2 ^ 0xF0);

--echo Testing that ^ has precedence over %
select 0xFF ^ 0xF0 % 0x20, (0xFF ^ 0xF0) % 0x20, 0xFF ^ (0xF0 % 0x20);
select 0xFF % 0x20 ^ 0xF0, (0xFF % 0x20) ^ 0xF0, 0xFF % (0x20 ^ 0xF0);

--echo Testing that ^ has precedence over DIV
select 0xFF ^ 0xF0 DIV 2, (0xFF ^ 0xF0) DIV 2, 0xFF ^ (0xF0 DIV 2);
select 0xF2 DIV 2 ^ 0xF0, (0xF2 DIV 2) ^ 0xF0, 0xF2 DIV (2 ^ 0xF0);

--echo Testing that ^ has precedence over MOD
select 0xFF ^ 0xF0 MOD 0x20, (0xFF ^ 0xF0) MOD 0x20, 0xFF ^ (0xF0 MOD 0x20);
select 0xFF MOD 0x20 ^ 0xF0, (0xFF MOD 0x20) ^ 0xF0, 0xFF MOD (0x20 ^ 0xF0);

