if (!`SELECT count(*) AS 'true' FROM information_schema.collations WHERE collation_name LIKE '$collation'`) {
   skip Test needs character set '$collation';
}
