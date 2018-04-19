if (`SELECT (IFNULL(@file,'') NOT LIKE '%\nde_DE.iso88591\n%')`)
{
  Skip Need POSIX locale de_DE.iso88591;
}

if (`SELECT (IFNULL(@file,'') NOT LIKE '%\nru_RU.koi8r\n%')`)
{
  Skip Need POSIX locale ru_RU.koi8r;
}

if (`SELECT (IFNULL(@file,'') NOT LIKE '%\ncs_CZ.iso88592\n%')`)
{
  Skip Need POSIX locale cs_CZ.iso88592;
}

if (`SELECT (IFNULL(@file,'') NOT LIKE '%\nen_US.utf8\n%')`)
{
  Skip Need POSIX locale en_US.utf8;
}
