select regexp_like('abc','abc') /* Result: y */;
regexp_like('abc','abc')
1
select regexp_like('xbc','abc') /* Result: n */;
regexp_like('xbc','abc')
0
select regexp_like('axc','abc') /* Result: n */;
regexp_like('axc','abc')
0
select regexp_like('abx','abc') /* Result: n */;
regexp_like('abx','abc')
0
select regexp_like('xabcy','abc') /* Result: y */;
regexp_like('xabcy','abc')
1
select regexp_like('ababc','abc') /* Result: y */;
regexp_like('ababc','abc')
1
select regexp_like('abc','ab*c') /* Result: y */;
regexp_like('abc','ab*c')
1
select regexp_like('abc','ab*bc') /* Result: y */;
regexp_like('abc','ab*bc')
1
select regexp_like('abbc','ab*bc') /* Result: y */;
regexp_like('abbc','ab*bc')
1
select regexp_like('abbbbc','ab*bc') /* Result: y */;
regexp_like('abbbbc','ab*bc')
1
select regexp_like('abbbbc','.{1}') /* Result: y */;
regexp_like('abbbbc','.{1}')
1
select regexp_like('abbbbc','.{3,4}') /* Result: y */;
regexp_like('abbbbc','.{3,4}')
1
select regexp_like('abbbbc','ab{0,}bc') /* Result: y */;
regexp_like('abbbbc','ab{0,}bc')
1
select regexp_like('abbc','ab+bc') /* Result: y */;
regexp_like('abbc','ab+bc')
1
select regexp_like('abc','ab+bc') /* Result: n */;
regexp_like('abc','ab+bc')
0
select regexp_like('abq','ab+bc') /* Result: n */;
regexp_like('abq','ab+bc')
0
select regexp_like('abq','ab{1,}bc') /* Result: n */;
regexp_like('abq','ab{1,}bc')
0
select regexp_like('abbbbc','ab+bc') /* Result: y */;
regexp_like('abbbbc','ab+bc')
1
select regexp_like('abbbbc','ab{1,}bc') /* Result: y */;
regexp_like('abbbbc','ab{1,}bc')
1
select regexp_like('abbbbc','ab{1,3}bc') /* Result: y */;
regexp_like('abbbbc','ab{1,3}bc')
1
select regexp_like('abbbbc','ab{3,4}bc') /* Result: y */;
regexp_like('abbbbc','ab{3,4}bc')
1
select regexp_like('abbbbc','ab{4,5}bc') /* Result: n */;
regexp_like('abbbbc','ab{4,5}bc')
0
select regexp_like('abbc','ab?bc') /* Result: y */;
regexp_like('abbc','ab?bc')
1
select regexp_like('abc','ab?bc') /* Result: y */;
regexp_like('abc','ab?bc')
1
select regexp_like('abc','ab{0,1}bc') /* Result: y */;
regexp_like('abc','ab{0,1}bc')
1
select regexp_like('abbbbc','ab?bc') /* Result: n */;
regexp_like('abbbbc','ab?bc')
0
select regexp_like('abc','ab?c') /* Result: y */;
regexp_like('abc','ab?c')
1
select regexp_like('abc','ab{0,1}c') /* Result: y */;
regexp_like('abc','ab{0,1}c')
1
select regexp_like('abc','^abc$') /* Result: y */;
regexp_like('abc','^abc$')
1
select regexp_like('abcc','^abc$') /* Result: n */;
regexp_like('abcc','^abc$')
0
select regexp_like('abcc','^abc') /* Result: y */;
regexp_like('abcc','^abc')
1
select regexp_like('aabc','^abc$') /* Result: n */;
regexp_like('aabc','^abc$')
0
select regexp_like('aabc','abc$') /* Result: y */;
regexp_like('aabc','abc$')
1
select regexp_like('aabcd','abc$') /* Result: n */;
regexp_like('aabcd','abc$')
0
select regexp_like('abc','^') /* Result: y */;
regexp_like('abc','^')
1
select regexp_like('abc','$') /* Result: y */;
regexp_like('abc','$')
1
select regexp_like('abc','a.c') /* Result: y */;
regexp_like('abc','a.c')
1
select regexp_like('axc','a.c') /* Result: y */;
regexp_like('axc','a.c')
1
select regexp_like('axyzc','a.*c') /* Result: y */;
regexp_like('axyzc','a.*c')
1
select regexp_like('axyzd','a.*c') /* Result: n */;
regexp_like('axyzd','a.*c')
0
select regexp_like('abc','a[bc]d') /* Result: n */;
regexp_like('abc','a[bc]d')
0
select regexp_like('abd','a[bc]d') /* Result: y */;
regexp_like('abd','a[bc]d')
1
select regexp_like('abd','a[b-d]e') /* Result: n */;
regexp_like('abd','a[b-d]e')
0
select regexp_like('ace','a[b-d]e') /* Result: y */;
regexp_like('ace','a[b-d]e')
1
select regexp_like('aac','a[b-d]') /* Result: y */;
regexp_like('aac','a[b-d]')
1
select regexp_like('a-','a[-b]') /* Result: y */;
regexp_like('a-','a[-b]')
1
select regexp_like('a-','a[b-]') /* Result: y */;
regexp_like('a-','a[b-]')
1
select regexp_like('-','a[b-a]') /* Result: c */;
ERROR HY000: The regular expression contains an [x-y] character range where x comes after y.
select regexp_like('-','a[]b') /* Result: ci */;
ERROR HY000: The regular expression contains an unclosed bracket expression.
select regexp_like('-','a[') /* Result: c */;
ERROR HY000: The regular expression contains an unclosed bracket expression.
select regexp_like('a]','a]') /* Result: y */;
regexp_like('a]','a]')
1
select regexp_like('a]b','a[]]b') /* Result: y */;
regexp_like('a]b','a[]]b')
1
select regexp_like('aed','a[^bc]d') /* Result: y */;
regexp_like('aed','a[^bc]d')
1
select regexp_like('abd','a[^bc]d') /* Result: n */;
regexp_like('abd','a[^bc]d')
0
select regexp_like('adc','a[^-b]c') /* Result: y */;
regexp_like('adc','a[^-b]c')
1
select regexp_like('a-c','a[^-b]c') /* Result: n */;
regexp_like('a-c','a[^-b]c')
0
select regexp_like('a]c','a[^]b]c') /* Result: n */;
regexp_like('a]c','a[^]b]c')
0
select regexp_like('adc','a[^]b]c') /* Result: y */;
regexp_like('adc','a[^]b]c')
1
select regexp_like('a-','\\ba\\b') /* Result: y */;
regexp_like('a-','\\ba\\b')
1
select regexp_like('-a','\\ba\\b') /* Result: y */;
regexp_like('-a','\\ba\\b')
1
select regexp_like('-a-','\\ba\\b') /* Result: y */;
regexp_like('-a-','\\ba\\b')
1
select regexp_like('xy','\\by\\b') /* Result: n */;
regexp_like('xy','\\by\\b')
0
select regexp_like('yz','\\by\\b') /* Result: n */;
regexp_like('yz','\\by\\b')
0
select regexp_like('xyz','\\by\\b') /* Result: n */;
regexp_like('xyz','\\by\\b')
0
select regexp_like('a-','\\Ba\\B') /* Result: n */;
regexp_like('a-','\\Ba\\B')
0
select regexp_like('-a','\\Ba\\B') /* Result: n */;
regexp_like('-a','\\Ba\\B')
0
select regexp_like('-a-','\\Ba\\B') /* Result: n */;
regexp_like('-a-','\\Ba\\B')
0
select regexp_like('xy','\\By\\b') /* Result: y */;
regexp_like('xy','\\By\\b')
1
select regexp_like('yz','\\by\\B') /* Result: y */;
regexp_like('yz','\\by\\B')
1
select regexp_like('xyz','\\By\\B') /* Result: y */;
regexp_like('xyz','\\By\\B')
1
select regexp_like('a','\\w') /* Result: y */;
regexp_like('a','\\w')
1
select regexp_like('-','\\w') /* Result: n */;
regexp_like('-','\\w')
0
select regexp_like('a','\\W') /* Result: n */;
regexp_like('a','\\W')
0
select regexp_like('-','\\W') /* Result: y */;
regexp_like('-','\\W')
1
select regexp_like('a b','a\\sb') /* Result: y */;
regexp_like('a b','a\\sb')
1
select regexp_like('a-b','a\\sb') /* Result: n */;
regexp_like('a-b','a\\sb')
0
select regexp_like('a b','a\\Sb') /* Result: n */;
regexp_like('a b','a\\Sb')
0
select regexp_like('a-b','a\\Sb') /* Result: y */;
regexp_like('a-b','a\\Sb')
1
select regexp_like('1','\\d') /* Result: y */;
regexp_like('1','\\d')
1
select regexp_like('-','\\d') /* Result: n */;
regexp_like('-','\\d')
0
select regexp_like('1','\\D') /* Result: n */;
regexp_like('1','\\D')
0
select regexp_like('-','\\D') /* Result: y */;
regexp_like('-','\\D')
1
select regexp_like('a','[\\w]') /* Result: y */;
regexp_like('a','[\\w]')
1
select regexp_like('-','[\\w]') /* Result: n */;
regexp_like('-','[\\w]')
0
select regexp_like('a','[\\W]') /* Result: n */;
regexp_like('a','[\\W]')
0
select regexp_like('-','[\\W]') /* Result: y */;
regexp_like('-','[\\W]')
1
select regexp_like('a b','a[\\s]b') /* Result: y */;
regexp_like('a b','a[\\s]b')
1
select regexp_like('a-b','a[\\s]b') /* Result: n */;
regexp_like('a-b','a[\\s]b')
0
select regexp_like('a b','a[\\S]b') /* Result: n */;
regexp_like('a b','a[\\S]b')
0
select regexp_like('a-b','a[\\S]b') /* Result: y */;
regexp_like('a-b','a[\\S]b')
1
select regexp_like('1','[\\d]') /* Result: y */;
regexp_like('1','[\\d]')
1
select regexp_like('-','[\\d]') /* Result: n */;
regexp_like('-','[\\d]')
0
select regexp_like('1','[\\D]') /* Result: n */;
regexp_like('1','[\\D]')
0
select regexp_like('-','[\\D]') /* Result: y */;
regexp_like('-','[\\D]')
1
select regexp_like('abc','ab|cd') /* Result: y */;
regexp_like('abc','ab|cd')
1
select regexp_like('abcd','ab|cd') /* Result: y */;
regexp_like('abcd','ab|cd')
1
select regexp_like('def','()ef') /* Result: y */;
regexp_like('def','()ef')
1
select regexp_like('-','*a') /* Result: c */;
ERROR HY000: Syntax error in regular expression on line 1, character 1.
select regexp_like('-','(*)b') /* Result: c */;
ERROR HY000: Syntax error in regular expression on line 1, character 2.
select regexp_like('b','$b') /* Result: n */;
regexp_like('b','$b')
0
select regexp_like('-','a\\') /* Result: c */;
ERROR HY000: Unrecognized escape sequence in regular expression.
select regexp_like('a(b','a\\(b') /* Result: y */;
regexp_like('a(b','a\\(b')
1
select regexp_like('ab','a\\(*b') /* Result: y */;
regexp_like('ab','a\\(*b')
1
select regexp_like('a((b','a\\(*b') /* Result: y */;
regexp_like('a((b','a\\(*b')
1
select regexp_like('a\\b','a\\\\b') /* Result: y */;
regexp_like('a\\b','a\\\\b')
1
select regexp_like('-','abc)') /* Result: c */;
ERROR HY000: Mismatched parenthesis in regular expression.
select regexp_like('-','(abc') /* Result: c */;
ERROR HY000: Mismatched parenthesis in regular expression.
select regexp_like('abc','((a))') /* Result: y */;
regexp_like('abc','((a))')
1
select regexp_like('abc','(a)b(c)') /* Result: y */;
regexp_like('abc','(a)b(c)')
1
select regexp_like('aabbabc','a+b+c') /* Result: y */;
regexp_like('aabbabc','a+b+c')
1
select regexp_like('aabbabc','a{1,}b{1,}c') /* Result: y */;
regexp_like('aabbabc','a{1,}b{1,}c')
1
select regexp_like('-','a**') /* Result: c */;
ERROR HY000: Syntax error in regular expression on line 1, character 3.
select regexp_like('abcabc','a.+?c') /* Result: y */;
regexp_like('abcabc','a.+?c')
1
select regexp_like('ab','(a+|b)*') /* Result: y */;
regexp_like('ab','(a+|b)*')
1
select regexp_like('ab','(a+|b){0,}') /* Result: y */;
regexp_like('ab','(a+|b){0,}')
1
select regexp_like('ab','(a+|b)+') /* Result: y */;
regexp_like('ab','(a+|b)+')
1
select regexp_like('ab','(a+|b){1,}') /* Result: y */;
regexp_like('ab','(a+|b){1,}')
1
select regexp_like('ab','(a+|b)?') /* Result: y */;
regexp_like('ab','(a+|b)?')
1
select regexp_like('ab','(a+|b){0,1}') /* Result: y */;
regexp_like('ab','(a+|b){0,1}')
1
select regexp_like('-',')(') /* Result: c */;
ERROR HY000: Mismatched parenthesis in regular expression.
select regexp_like('cde','[^ab]*') /* Result: y */;
regexp_like('cde','[^ab]*')
1
select regexp_like('','abc') /* Result: n */;
regexp_like('','abc')
0
select regexp_like('','a*') /* Result: y */;
regexp_like('','a*')
1
select regexp_like('abbbcd','([abc])*d') /* Result: y */;
regexp_like('abbbcd','([abc])*d')
1
select regexp_like('abcd','([abc])*bcd') /* Result: y */;
regexp_like('abcd','([abc])*bcd')
1
select regexp_like('e','a|b|c|d|e') /* Result: y */;
regexp_like('e','a|b|c|d|e')
1
select regexp_like('ef','(a|b|c|d|e)f') /* Result: y */;
regexp_like('ef','(a|b|c|d|e)f')
1
select regexp_like('abcdefg','abcd*efg') /* Result: y */;
regexp_like('abcdefg','abcd*efg')
1
select regexp_like('xabyabbbz','ab*') /* Result: y */;
regexp_like('xabyabbbz','ab*')
1
select regexp_like('xayabbbz','ab*') /* Result: y */;
regexp_like('xayabbbz','ab*')
1
select regexp_like('abcde','(ab|cd)e') /* Result: y */;
regexp_like('abcde','(ab|cd)e')
1
select regexp_like('hij','[abhgefdc]ij') /* Result: y */;
regexp_like('hij','[abhgefdc]ij')
1
select regexp_like('abcde','^(ab|cd)e') /* Result: n */;
regexp_like('abcde','^(ab|cd)e')
0
select regexp_like('abcdef','(abc|)ef') /* Result: y */;
regexp_like('abcdef','(abc|)ef')
1
select regexp_like('abcd','(a|b)c*d') /* Result: y */;
regexp_like('abcd','(a|b)c*d')
1
select regexp_like('abc','(ab|ab*)bc') /* Result: y */;
regexp_like('abc','(ab|ab*)bc')
1
select regexp_like('abc','a([bc]*)c*') /* Result: y */;
regexp_like('abc','a([bc]*)c*')
1
select regexp_like('abcd','a([bc]*)(c*d)') /* Result: y */;
regexp_like('abcd','a([bc]*)(c*d)')
1
select regexp_like('abcd','a([bc]+)(c*d)') /* Result: y */;
regexp_like('abcd','a([bc]+)(c*d)')
1
select regexp_like('abcd','a([bc]*)(c+d)') /* Result: y */;
regexp_like('abcd','a([bc]*)(c+d)')
1
select regexp_like('adcdcde','a[bcd]*dcdcde') /* Result: y */;
regexp_like('adcdcde','a[bcd]*dcdcde')
1
select regexp_like('adcdcde','a[bcd]+dcdcde') /* Result: n */;
regexp_like('adcdcde','a[bcd]+dcdcde')
0
select regexp_like('abc','(ab|a)b*c') /* Result: y */;
regexp_like('abc','(ab|a)b*c')
1
select regexp_like('abcd','((a)(b)c)(d)') /* Result: y */;
regexp_like('abcd','((a)(b)c)(d)')
1
select regexp_like('alpha','[a-zA-Z_][a-zA-Z0-9_]*') /* Result: y */;
regexp_like('alpha','[a-zA-Z_][a-zA-Z0-9_]*')
1
select regexp_like('abh','^a(bc+|b[eh])g|.h$') /* Result: y */;
regexp_like('abh','^a(bc+|b[eh])g|.h$')
1
select regexp_like('effgz','(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
regexp_like('effgz','(bc+d$|ef*g.|h?i(j|k))')
1
select regexp_like('ij','(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
regexp_like('ij','(bc+d$|ef*g.|h?i(j|k))')
1
select regexp_like('effg','(bc+d$|ef*g.|h?i(j|k))') /* Result: n */;
regexp_like('effg','(bc+d$|ef*g.|h?i(j|k))')
0
select regexp_like('bcdd','(bc+d$|ef*g.|h?i(j|k))') /* Result: n */;
regexp_like('bcdd','(bc+d$|ef*g.|h?i(j|k))')
0
select regexp_like('reffgz','(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
regexp_like('reffgz','(bc+d$|ef*g.|h?i(j|k))')
1
select regexp_like('a','((((((((((a))))))))))') /* Result: y */;
regexp_like('a','((((((((((a))))))))))')
1
select regexp_like('aa','((((((((((a))))))))))\\10') /* Result: y */;
regexp_like('aa','((((((((((a))))))))))\\10')
1
select regexp_like('a','(((((((((a)))))))))') /* Result: y */;
regexp_like('a','(((((((((a)))))))))')
1
select regexp_like('uh-uh','multiple words of text') /* Result: n */;
regexp_like('uh-uh','multiple words of text')
0
select regexp_like('multiple words, yeah','multiple words') /* Result: y */;
regexp_like('multiple words, yeah','multiple words')
1
select regexp_like('abcde','(.*)c(.*)') /* Result: y */;
regexp_like('abcde','(.*)c(.*)')
1
select regexp_like('(a, b)','\\((.*), (.*)\\)') /* Result: y */;
regexp_like('(a, b)','\\((.*), (.*)\\)')
1
select regexp_like('ab','[k]') /* Result: n */;
regexp_like('ab','[k]')
0
select regexp_like('abcd','abcd') /* Result: y */;
regexp_like('abcd','abcd')
1
select regexp_like('abcd','a(bc)d') /* Result: y */;
regexp_like('abcd','a(bc)d')
1
select regexp_like('ac','a[-]?c') /* Result: y */;
regexp_like('ac','a[-]?c')
1
select regexp_like('abcabc','(abc)\\1') /* Result: y */;
regexp_like('abcabc','(abc)\\1')
1
select regexp_like('abcabc','([a-c]*)\\1') /* Result: y */;
regexp_like('abcabc','([a-c]*)\\1')
1
select regexp_like('-','\\1') /* Result: c */;
ERROR HY000: Invalid back-reference in regular expression.
select regexp_like('-','\\2') /* Result: c */;
ERROR HY000: Invalid back-reference in regular expression.
select regexp_like('a','(a)|\\1') /* Result: y */;
regexp_like('a','(a)|\\1')
1
select regexp_like('x','(a)|\\1') /* Result: n */;
regexp_like('x','(a)|\\1')
0
select regexp_like('-','(a)|\\2') /* Result: c */;
ERROR HY000: Invalid back-reference in regular expression.
select regexp_like('ababbbcbc','(([a-c])b*?\\2)*') /* Result: y */;
regexp_like('ababbbcbc','(([a-c])b*?\\2)*')
1
select regexp_like('ababbbcbc','(([a-c])b*?\\2){3}') /* Result: y */;
regexp_like('ababbbcbc','(([a-c])b*?\\2){3}')
1
select regexp_like('aaxabxbaxbbx','((\\3|b)\\2(a)x)+') /* Result: n */;
regexp_like('aaxabxbaxbbx','((\\3|b)\\2(a)x)+')
0
select regexp_like('aaaxabaxbaaxbbax','((\\3|b)\\2(a)x)+') /* Result: y */;
regexp_like('aaaxabaxbaaxbbax','((\\3|b)\\2(a)x)+')
1
select regexp_like('bbaababbabaaaaabbaaaabba','((\\3|b)\\2(a)){2,}') /* Result: y */;
regexp_like('bbaababbabaaaaabbaaaabba','((\\3|b)\\2(a)){2,}')
1
select regexp_like('b','(a)|(b)') /* Result: y */;
regexp_like('b','(a)|(b)')
1
select regexp_like('ABC','(?i)abc') /* Result: y */;
regexp_like('ABC','(?i)abc')
1
select regexp_like('XBC','(?i)abc') /* Result: n */;
regexp_like('XBC','(?i)abc')
0
select regexp_like('AXC','(?i)abc') /* Result: n */;
regexp_like('AXC','(?i)abc')
0
select regexp_like('ABX','(?i)abc') /* Result: n */;
regexp_like('ABX','(?i)abc')
0
select regexp_like('XABCY','(?i)abc') /* Result: y */;
regexp_like('XABCY','(?i)abc')
1
select regexp_like('ABABC','(?i)abc') /* Result: y */;
regexp_like('ABABC','(?i)abc')
1
select regexp_like('ABC','(?i)ab*c') /* Result: y */;
regexp_like('ABC','(?i)ab*c')
1
select regexp_like('ABC','(?i)ab*bc') /* Result: y */;
regexp_like('ABC','(?i)ab*bc')
1
select regexp_like('ABBC','(?i)ab*bc') /* Result: y */;
regexp_like('ABBC','(?i)ab*bc')
1
select regexp_like('ABBBBC','(?i)ab*?bc') /* Result: y */;
regexp_like('ABBBBC','(?i)ab*?bc')
1
select regexp_like('ABBBBC','(?i)ab{0,}?bc') /* Result: y */;
regexp_like('ABBBBC','(?i)ab{0,}?bc')
1
select regexp_like('ABBC','(?i)ab+?bc') /* Result: y */;
regexp_like('ABBC','(?i)ab+?bc')
1
select regexp_like('ABC','(?i)ab+bc') /* Result: n */;
regexp_like('ABC','(?i)ab+bc')
0
select regexp_like('ABQ','(?i)ab+bc') /* Result: n */;
regexp_like('ABQ','(?i)ab+bc')
0
select regexp_like('ABQ','(?i)ab{1,}bc') /* Result: n */;
regexp_like('ABQ','(?i)ab{1,}bc')
0
select regexp_like('ABBBBC','(?i)ab+bc') /* Result: y */;
regexp_like('ABBBBC','(?i)ab+bc')
1
select regexp_like('ABBBBC','(?i)ab{1,}?bc') /* Result: y */;
regexp_like('ABBBBC','(?i)ab{1,}?bc')
1
select regexp_like('ABBBBC','(?i)ab{1,3}?bc') /* Result: y */;
regexp_like('ABBBBC','(?i)ab{1,3}?bc')
1
select regexp_like('ABBBBC','(?i)ab{3,4}?bc') /* Result: y */;
regexp_like('ABBBBC','(?i)ab{3,4}?bc')
1
select regexp_like('ABBBBC','(?i)ab{4,5}?bc') /* Result: n */;
regexp_like('ABBBBC','(?i)ab{4,5}?bc')
0
select regexp_like('ABBC','(?i)ab??bc') /* Result: y */;
regexp_like('ABBC','(?i)ab??bc')
1
select regexp_like('ABC','(?i)ab??bc') /* Result: y */;
regexp_like('ABC','(?i)ab??bc')
1
select regexp_like('ABC','(?i)ab{0,1}?bc') /* Result: y */;
regexp_like('ABC','(?i)ab{0,1}?bc')
1
select regexp_like('ABBBBC','(?i)ab??bc') /* Result: n */;
regexp_like('ABBBBC','(?i)ab??bc')
0
select regexp_like('ABC','(?i)ab??c') /* Result: y */;
regexp_like('ABC','(?i)ab??c')
1
select regexp_like('ABC','(?i)ab{0,1}?c') /* Result: y */;
regexp_like('ABC','(?i)ab{0,1}?c')
1
select regexp_like('ABC','(?i)^abc$') /* Result: y */;
regexp_like('ABC','(?i)^abc$')
1
select regexp_like('ABCC','(?i)^abc$') /* Result: n */;
regexp_like('ABCC','(?i)^abc$')
0
select regexp_like('ABCC','(?i)^abc') /* Result: y */;
regexp_like('ABCC','(?i)^abc')
1
select regexp_like('AABC','(?i)^abc$') /* Result: n */;
regexp_like('AABC','(?i)^abc$')
0
select regexp_like('AABC','(?i)abc$') /* Result: y */;
regexp_like('AABC','(?i)abc$')
1
select regexp_like('ABC','(?i)^') /* Result: y */;
regexp_like('ABC','(?i)^')
1
select regexp_like('ABC','(?i)$') /* Result: y */;
regexp_like('ABC','(?i)$')
1
select regexp_like('ABC','(?i)a.c') /* Result: y */;
regexp_like('ABC','(?i)a.c')
1
select regexp_like('AXC','(?i)a.c') /* Result: y */;
regexp_like('AXC','(?i)a.c')
1
select regexp_like('AXYZC','(?i)a.*?c') /* Result: y */;
regexp_like('AXYZC','(?i)a.*?c')
1
select regexp_like('AXYZD','(?i)a.*c') /* Result: n */;
regexp_like('AXYZD','(?i)a.*c')
0
select regexp_like('ABC','(?i)a[bc]d') /* Result: n */;
regexp_like('ABC','(?i)a[bc]d')
0
select regexp_like('ABD','(?i)a[bc]d') /* Result: y */;
regexp_like('ABD','(?i)a[bc]d')
1
select regexp_like('ABD','(?i)a[b-d]e') /* Result: n */;
regexp_like('ABD','(?i)a[b-d]e')
0
select regexp_like('ACE','(?i)a[b-d]e') /* Result: y */;
regexp_like('ACE','(?i)a[b-d]e')
1
select regexp_like('AAC','(?i)a[b-d]') /* Result: y */;
regexp_like('AAC','(?i)a[b-d]')
1
select regexp_like('A-','(?i)a[-b]') /* Result: y */;
regexp_like('A-','(?i)a[-b]')
1
select regexp_like('A-','(?i)a[b-]') /* Result: y */;
regexp_like('A-','(?i)a[b-]')
1
select regexp_like('-','(?i)a[b-a]') /* Result: c */;
ERROR HY000: The regular expression contains an [x-y] character range where x comes after y.
select regexp_like('-','(?i)a[]b') /* Result: ci */;
ERROR HY000: The regular expression contains an unclosed bracket expression.
select regexp_like('-','(?i)a[') /* Result: c */;
ERROR HY000: The regular expression contains an unclosed bracket expression.
select regexp_like('A]','(?i)a]') /* Result: y */;
regexp_like('A]','(?i)a]')
1
select regexp_like('A]B','(?i)a[]]b') /* Result: y */;
regexp_like('A]B','(?i)a[]]b')
1
select regexp_like('AED','(?i)a[^bc]d') /* Result: y */;
regexp_like('AED','(?i)a[^bc]d')
1
select regexp_like('ABD','(?i)a[^bc]d') /* Result: n */;
regexp_like('ABD','(?i)a[^bc]d')
0
select regexp_like('ADC','(?i)a[^-b]c') /* Result: y */;
regexp_like('ADC','(?i)a[^-b]c')
1
select regexp_like('A-C','(?i)a[^-b]c') /* Result: n */;
regexp_like('A-C','(?i)a[^-b]c')
0
select regexp_like('A]C','(?i)a[^]b]c') /* Result: n */;
regexp_like('A]C','(?i)a[^]b]c')
0
select regexp_like('ADC','(?i)a[^]b]c') /* Result: y */;
regexp_like('ADC','(?i)a[^]b]c')
1
select regexp_like('ABC','(?i)ab|cd') /* Result: y */;
regexp_like('ABC','(?i)ab|cd')
1
select regexp_like('ABCD','(?i)ab|cd') /* Result: y */;
regexp_like('ABCD','(?i)ab|cd')
1
select regexp_like('DEF','(?i)()ef') /* Result: y */;
regexp_like('DEF','(?i)()ef')
1
select regexp_like('-','(?i)*a') /* Result: c */;
ERROR HY000: Syntax error in regular expression on line 1, character 5.
select regexp_like('-','(?i)(*)b') /* Result: c */;
ERROR HY000: Syntax error in regular expression on line 1, character 6.
select regexp_like('B','(?i)$b') /* Result: n */;
regexp_like('B','(?i)$b')
0
select regexp_like('-','(?i)a\\') /* Result: c */;
ERROR HY000: Unrecognized escape sequence in regular expression.
select regexp_like('A(B','(?i)a\\(b') /* Result: y */;
regexp_like('A(B','(?i)a\\(b')
1
select regexp_like('AB','(?i)a\\(*b') /* Result: y */;
regexp_like('AB','(?i)a\\(*b')
1
select regexp_like('A((B','(?i)a\\(*b') /* Result: y */;
regexp_like('A((B','(?i)a\\(*b')
1
select regexp_like('A\\B','(?i)a\\\\b') /* Result: y */;
regexp_like('A\\B','(?i)a\\\\b')
1
select regexp_like('-','(?i)abc)') /* Result: c */;
ERROR HY000: Mismatched parenthesis in regular expression.
select regexp_like('-','(?i)(abc') /* Result: c */;
ERROR HY000: Mismatched parenthesis in regular expression.
select regexp_like('ABC','(?i)((a))') /* Result: y */;
regexp_like('ABC','(?i)((a))')
1
select regexp_like('ABC','(?i)(a)b(c)') /* Result: y */;
regexp_like('ABC','(?i)(a)b(c)')
1
select regexp_like('AABBABC','(?i)a+b+c') /* Result: y */;
regexp_like('AABBABC','(?i)a+b+c')
1
select regexp_like('AABBABC','(?i)a{1,}b{1,}c') /* Result: y */;
regexp_like('AABBABC','(?i)a{1,}b{1,}c')
1
select regexp_like('-','(?i)a**') /* Result: c */;
ERROR HY000: Syntax error in regular expression on line 1, character 7.
select regexp_like('ABCABC','(?i)a.+?c') /* Result: y */;
regexp_like('ABCABC','(?i)a.+?c')
1
select regexp_like('ABCABC','(?i)a.*?c') /* Result: y */;
regexp_like('ABCABC','(?i)a.*?c')
1
select regexp_like('ABCABC','(?i)a.{0,5}?c') /* Result: y */;
regexp_like('ABCABC','(?i)a.{0,5}?c')
1
select regexp_like('AB','(?i)(a+|b)*') /* Result: y */;
regexp_like('AB','(?i)(a+|b)*')
1
select regexp_like('AB','(?i)(a+|b){0,}') /* Result: y */;
regexp_like('AB','(?i)(a+|b){0,}')
1
select regexp_like('AB','(?i)(a+|b)+') /* Result: y */;
regexp_like('AB','(?i)(a+|b)+')
1
select regexp_like('AB','(?i)(a+|b){1,}') /* Result: y */;
regexp_like('AB','(?i)(a+|b){1,}')
1
select regexp_like('AB','(?i)(a+|b)?') /* Result: y */;
regexp_like('AB','(?i)(a+|b)?')
1
select regexp_like('AB','(?i)(a+|b){0,1}') /* Result: y */;
regexp_like('AB','(?i)(a+|b){0,1}')
1
select regexp_like('AB','(?i)(a+|b){0,1}?') /* Result: y */;
regexp_like('AB','(?i)(a+|b){0,1}?')
1
select regexp_like('-','(?i))(') /* Result: c */;
ERROR HY000: Mismatched parenthesis in regular expression.
select regexp_like('CDE','(?i)[^ab]*') /* Result: y */;
regexp_like('CDE','(?i)[^ab]*')
1
select regexp_like('','(?i)abc') /* Result: n */;
regexp_like('','(?i)abc')
0
select regexp_like('','(?i)a*') /* Result: y */;
regexp_like('','(?i)a*')
1
select regexp_like('ABBBCD','(?i)([abc])*d') /* Result: y */;
regexp_like('ABBBCD','(?i)([abc])*d')
1
select regexp_like('ABCD','(?i)([abc])*bcd') /* Result: y */;
regexp_like('ABCD','(?i)([abc])*bcd')
1
select regexp_like('E','(?i)a|b|c|d|e') /* Result: y */;
regexp_like('E','(?i)a|b|c|d|e')
1
select regexp_like('EF','(?i)(a|b|c|d|e)f') /* Result: y */;
regexp_like('EF','(?i)(a|b|c|d|e)f')
1
select regexp_like('ABCDEFG','(?i)abcd*efg') /* Result: y */;
regexp_like('ABCDEFG','(?i)abcd*efg')
1
select regexp_like('XABYABBBZ','(?i)ab*') /* Result: y */;
regexp_like('XABYABBBZ','(?i)ab*')
1
select regexp_like('XAYABBBZ','(?i)ab*') /* Result: y */;
regexp_like('XAYABBBZ','(?i)ab*')
1
select regexp_like('ABCDE','(?i)(ab|cd)e') /* Result: y */;
regexp_like('ABCDE','(?i)(ab|cd)e')
1
select regexp_like('HIJ','(?i)[abhgefdc]ij') /* Result: y */;
regexp_like('HIJ','(?i)[abhgefdc]ij')
1
select regexp_like('ABCDE','(?i)^(ab|cd)e') /* Result: n */;
regexp_like('ABCDE','(?i)^(ab|cd)e')
0
select regexp_like('ABCDEF','(?i)(abc|)ef') /* Result: y */;
regexp_like('ABCDEF','(?i)(abc|)ef')
1
select regexp_like('ABCD','(?i)(a|b)c*d') /* Result: y */;
regexp_like('ABCD','(?i)(a|b)c*d')
1
select regexp_like('ABC','(?i)(ab|ab*)bc') /* Result: y */;
regexp_like('ABC','(?i)(ab|ab*)bc')
1
select regexp_like('ABC','(?i)a([bc]*)c*') /* Result: y */;
regexp_like('ABC','(?i)a([bc]*)c*')
1
select regexp_like('ABCD','(?i)a([bc]*)(c*d)') /* Result: y */;
regexp_like('ABCD','(?i)a([bc]*)(c*d)')
1
select regexp_like('ABCD','(?i)a([bc]+)(c*d)') /* Result: y */;
regexp_like('ABCD','(?i)a([bc]+)(c*d)')
1
select regexp_like('ABCD','(?i)a([bc]*)(c+d)') /* Result: y */;
regexp_like('ABCD','(?i)a([bc]*)(c+d)')
1
select regexp_like('ADCDCDE','(?i)a[bcd]*dcdcde') /* Result: y */;
regexp_like('ADCDCDE','(?i)a[bcd]*dcdcde')
1
select regexp_like('ADCDCDE','(?i)a[bcd]+dcdcde') /* Result: n */;
regexp_like('ADCDCDE','(?i)a[bcd]+dcdcde')
0
select regexp_like('ABC','(?i)(ab|a)b*c') /* Result: y */;
regexp_like('ABC','(?i)(ab|a)b*c')
1
select regexp_like('ABCD','(?i)((a)(b)c)(d)') /* Result: y */;
regexp_like('ABCD','(?i)((a)(b)c)(d)')
1
select regexp_like('ALPHA','(?i)[a-zA-Z_][a-zA-Z0-9_]*') /* Result: y */;
regexp_like('ALPHA','(?i)[a-zA-Z_][a-zA-Z0-9_]*')
1
select regexp_like('ABH','(?i)^a(bc+|b[eh])g|.h$') /* Result: y */;
regexp_like('ABH','(?i)^a(bc+|b[eh])g|.h$')
1
select regexp_like('EFFGZ','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
regexp_like('EFFGZ','(?i)(bc+d$|ef*g.|h?i(j|k))')
1
select regexp_like('IJ','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
regexp_like('IJ','(?i)(bc+d$|ef*g.|h?i(j|k))')
1
select regexp_like('EFFG','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: n */;
regexp_like('EFFG','(?i)(bc+d$|ef*g.|h?i(j|k))')
0
select regexp_like('BCDD','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: n */;
regexp_like('BCDD','(?i)(bc+d$|ef*g.|h?i(j|k))')
0
select regexp_like('REFFGZ','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
regexp_like('REFFGZ','(?i)(bc+d$|ef*g.|h?i(j|k))')
1
select regexp_like('A','(?i)((((((((((a))))))))))') /* Result: y */;
regexp_like('A','(?i)((((((((((a))))))))))')
1
select regexp_like('AA','(?i)((((((((((a))))))))))\\10') /* Result: y */;
regexp_like('AA','(?i)((((((((((a))))))))))\\10')
1
select regexp_like('A','(?i)(((((((((a)))))))))') /* Result: y */;
regexp_like('A','(?i)(((((((((a)))))))))')
1
select regexp_like('A','(?i)(?:(?:(?:(?:(?:(?:(?:(?:(?:(a))))))))))') /* Result: y */;
regexp_like('A','(?i)(?:(?:(?:(?:(?:(?:(?:(?:(?:(a))))))))))')
1
select regexp_like('C','(?i)(?:(?:(?:(?:(?:(?:(?:(?:(?:(a|b|c))))))))))') /* Result: y */;
regexp_like('C','(?i)(?:(?:(?:(?:(?:(?:(?:(?:(?:(a|b|c))))))))))')
1
select regexp_like('UH-UH','(?i)multiple words of text') /* Result: n */;
regexp_like('UH-UH','(?i)multiple words of text')
0
select regexp_like('MULTIPLE WORDS, YEAH','(?i)multiple words') /* Result: y */;
regexp_like('MULTIPLE WORDS, YEAH','(?i)multiple words')
1
select regexp_like('ABCDE','(?i)(.*)c(.*)') /* Result: y */;
regexp_like('ABCDE','(?i)(.*)c(.*)')
1
select regexp_like('(A, B)','(?i)\\((.*), (.*)\\)') /* Result: y */;
regexp_like('(A, B)','(?i)\\((.*), (.*)\\)')
1
select regexp_like('AB','(?i)[k]') /* Result: n */;
regexp_like('AB','(?i)[k]')
0
select regexp_like('ABCD','(?i)abcd') /* Result: y */;
regexp_like('ABCD','(?i)abcd')
1
select regexp_like('ABCD','(?i)a(bc)d') /* Result: y */;
regexp_like('ABCD','(?i)a(bc)d')
1
select regexp_like('AC','(?i)a[-]?c') /* Result: y */;
regexp_like('AC','(?i)a[-]?c')
1
select regexp_like('ABCABC','(?i)(abc)\\1') /* Result: y */;
regexp_like('ABCABC','(?i)(abc)\\1')
1
select regexp_like('ABCABC','(?i)([a-c]*)\\1') /* Result: y */;
regexp_like('ABCABC','(?i)([a-c]*)\\1')
1
select regexp_like('abad','a(?!b).') /* Result: y */;
regexp_like('abad','a(?!b).')
1
select regexp_like('abad','a(?=d).') /* Result: y */;
regexp_like('abad','a(?=d).')
1
select regexp_like('abad','a(?=c|d).') /* Result: y */;
regexp_like('abad','a(?=c|d).')
1
select regexp_like('ace','a(?:b|c|d)(.)') /* Result: y */;
regexp_like('ace','a(?:b|c|d)(.)')
1
select regexp_like('ace','a(?:b|c|d)*(.)') /* Result: y */;
regexp_like('ace','a(?:b|c|d)*(.)')
1
select regexp_like('ace','a(?:b|c|d)+?(.)') /* Result: y */;
regexp_like('ace','a(?:b|c|d)+?(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d)+?(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d)+?(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d)+(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d)+(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d){2}(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){2}(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d){4,5}(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){4,5}(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d){4,5}?(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){4,5}?(.)')
1
select regexp_like('foobar','((foo)|(bar))*') /* Result: y */;
regexp_like('foobar','((foo)|(bar))*')
1
select regexp_like('-',':(?:') /* Result: c */;
ERROR HY000: Mismatched parenthesis in regular expression.
select regexp_like('acdbcdbe','a(?:b|c|d){6,7}(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){6,7}(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d){6,7}?(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){6,7}?(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d){5,6}(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){5,6}(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d){5,6}?(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){5,6}?(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d){5,7}(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){5,7}(.)')
1
select regexp_like('acdbcdbe','a(?:b|c|d){5,7}?(.)') /* Result: y */;
regexp_like('acdbcdbe','a(?:b|c|d){5,7}?(.)')
1
select regexp_like('ace','a(?:b|(c|e){1,2}?|d)+?(.)') /* Result: y */;
regexp_like('ace','a(?:b|(c|e){1,2}?|d)+?(.)')
1
select regexp_like('AB','^(.+)?B') /* Result: y */;
regexp_like('AB','^(.+)?B')
1
select regexp_like('.','^([^a-z])|(\\^)$') /* Result: y */;
regexp_like('.','^([^a-z])|(\\^)$')
1
select regexp_like('<&OUT','^[<>]&') /* Result: y */;
regexp_like('<&OUT','^[<>]&')
1
select regexp_like('aaaaaaaaaa','^(a\\1?){4}$') /* Result: y */;
regexp_like('aaaaaaaaaa','^(a\\1?){4}$')
1
select regexp_like('aaaaaaaaa','^(a\\1?){4}$') /* Result: n */;
regexp_like('aaaaaaaaa','^(a\\1?){4}$')
0
select regexp_like('aaaaaaaaaaa','^(a\\1?){4}$') /* Result: n */;
regexp_like('aaaaaaaaaaa','^(a\\1?){4}$')
0
select regexp_like('aaaaaaaaaa','^(a(?(1)\\1)){4}$') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('aaaaaaaaa','^(a(?(1)\\1)){4}$') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('aaaaaaaaaaa','^(a(?(1)\\1)){4}$') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('aaaaaaaaa','((a{4})+)') /* Result: y */;
regexp_like('aaaaaaaaa','((a{4})+)')
1
select regexp_like('aaaaaaaaaa','(((aa){2})+)') /* Result: y */;
regexp_like('aaaaaaaaaa','(((aa){2})+)')
1
select regexp_like('aaaaaaaaaa','(((a{2}){2})+)') /* Result: y */;
regexp_like('aaaaaaaaaa','(((a{2}){2})+)')
1
select regexp_like('foobar','(?:(f)(o)(o)|(b)(a)(r))*') /* Result: y */;
regexp_like('foobar','(?:(f)(o)(o)|(b)(a)(r))*')
1
select regexp_like('ab','(?<=a)b') /* Result: y */;
regexp_like('ab','(?<=a)b')
1
select regexp_like('cb','(?<=a)b') /* Result: n */;
regexp_like('cb','(?<=a)b')
0
select regexp_like('b','(?<=a)b') /* Result: n */;
regexp_like('b','(?<=a)b')
0
select regexp_like('ab','(?<!c)b') /* Result: y */;
regexp_like('ab','(?<!c)b')
1
select regexp_like('cb','(?<!c)b') /* Result: n */;
regexp_like('cb','(?<!c)b')
0
select regexp_like('b','(?<!c)b') /* Result: y */;
regexp_like('b','(?<!c)b')
1
select regexp_like('-','(?<%)b') /* Result: c */;
ERROR HY000: Syntax error in regular expression on line 1, character 4.
select regexp_like('aba','(?:..)*a') /* Result: y */;
regexp_like('aba','(?:..)*a')
1
select regexp_like('aba','(?:..)*?a') /* Result: y */;
regexp_like('aba','(?:..)*?a')
1
select regexp_like('abc','^(?:b|a(?=(.)))*\\1') /* Result: y */;
regexp_like('abc','^(?:b|a(?=(.)))*\\1')
1
select regexp_like('abc','^(){3,5}') /* Result: y */;
regexp_like('abc','^(){3,5}')
1
select regexp_like('aax','^(a+)*ax') /* Result: y */;
regexp_like('aax','^(a+)*ax')
1
select regexp_like('aax','^((a|b)+)*ax') /* Result: y */;
regexp_like('aax','^((a|b)+)*ax')
1
select regexp_like('aax','^((a|bc)+)*ax') /* Result: y */;
regexp_like('aax','^((a|bc)+)*ax')
1
select regexp_like('cab','(a|x)*ab') /* Result: y */;
regexp_like('cab','(a|x)*ab')
1
select regexp_like('cab','(a)*ab') /* Result: y */;
regexp_like('cab','(a)*ab')
1
select regexp_like('ab','(?:(?i)a)b') /* Result: y */;
regexp_like('ab','(?:(?i)a)b')
1
select regexp_like('ab','((?i)a)b') /* Result: y */;
regexp_like('ab','((?i)a)b')
1
select regexp_like('Ab','(?:(?i)a)b') /* Result: y */;
regexp_like('Ab','(?:(?i)a)b')
1
select regexp_like('Ab','((?i)a)b') /* Result: y */;
regexp_like('Ab','((?i)a)b')
1
select regexp_like('aB','(?:(?i)a)b') /* Result: n */;
regexp_like('aB','(?:(?i)a)b')
1
select regexp_like('aB','((?i)a)b') /* Result: n */;
regexp_like('aB','((?i)a)b')
1
select regexp_like('ab','(?i:a)b') /* Result: y */;
regexp_like('ab','(?i:a)b')
1
select regexp_like('ab','((?i:a))b') /* Result: y */;
regexp_like('ab','((?i:a))b')
1
select regexp_like('Ab','(?i:a)b') /* Result: y */;
regexp_like('Ab','(?i:a)b')
1
select regexp_like('Ab','((?i:a))b') /* Result: y */;
regexp_like('Ab','((?i:a))b')
1
select regexp_like('aB','(?i:a)b') /* Result: n */;
regexp_like('aB','(?i:a)b')
1
select regexp_like('aB','((?i:a))b') /* Result: n */;
regexp_like('aB','((?i:a))b')
1
select regexp_like('ab','(?i)(?:(?-i)a)b') /* Result: y */;
regexp_like('ab','(?i)(?:(?-i)a)b')
1
select regexp_like('ab','(?i)((?-i)a)b') /* Result: y */;
regexp_like('ab','(?i)((?-i)a)b')
1
select regexp_like('aB','(?i)(?:(?-i)a)b') /* Result: y */;
regexp_like('aB','(?i)(?:(?-i)a)b')
1
select regexp_like('aB','(?i)((?-i)a)b') /* Result: y */;
regexp_like('aB','(?i)((?-i)a)b')
1
select regexp_like('Ab','(?i)(?:(?-i)a)b') /* Result: n */;
regexp_like('Ab','(?i)(?:(?-i)a)b')
0
select regexp_like('Ab','(?i)((?-i)a)b') /* Result: n */;
regexp_like('Ab','(?i)((?-i)a)b')
0
select regexp_like('AB','(?i)(?:(?-i)a)b') /* Result: n */;
regexp_like('AB','(?i)(?:(?-i)a)b')
0
select regexp_like('AB','(?i)((?-i)a)b') /* Result: n */;
regexp_like('AB','(?i)((?-i)a)b')
0
select regexp_like('ab','(?i)(?-i:a)b') /* Result: y */;
regexp_like('ab','(?i)(?-i:a)b')
1
select regexp_like('ab','(?i)((?-i:a))b') /* Result: y */;
regexp_like('ab','(?i)((?-i:a))b')
1
select regexp_like('aB','(?i)(?-i:a)b') /* Result: y */;
regexp_like('aB','(?i)(?-i:a)b')
1
select regexp_like('aB','(?i)((?-i:a))b') /* Result: y */;
regexp_like('aB','(?i)((?-i:a))b')
1
select regexp_like('Ab','(?i)(?-i:a)b') /* Result: n */;
regexp_like('Ab','(?i)(?-i:a)b')
0
select regexp_like('Ab','(?i)((?-i:a))b') /* Result: n */;
regexp_like('Ab','(?i)((?-i:a))b')
0
select regexp_like('AB','(?i)(?-i:a)b') /* Result: n */;
regexp_like('AB','(?i)(?-i:a)b')
0
select regexp_like('AB','(?i)((?-i:a))b') /* Result: n */;
regexp_like('AB','(?i)((?-i:a))b')
0
select regexp_like('a\nB','(?i)((?-i:a.))b') /* Result: n */;
regexp_like('a\nB','(?i)((?-i:a.))b')
0
select regexp_like('a\nB','(?i)((?s-i:a.))b') /* Result: y */;
regexp_like('a\nB','(?i)((?s-i:a.))b')
1
select regexp_like('B\nB','(?i)((?s-i:a.))b') /* Result: n */;
regexp_like('B\nB','(?i)((?s-i:a.))b')
0
select regexp_like('cabbbb','(?:c|d)(?:)(?:a(?:)(?:b)(?:b(?:))(?:b(?:)(?:b)))') /* Result: y */;
regexp_like('cabbbb','(?:c|d)(?:)(?:a(?:)(?:b)(?:b(?:))(?:b(?:)(?:b)))')
1
select regexp_like('caaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','(?:c|d)(?:)(?:aaaaaaaa(?:)(?:bbbbbbbb)(?:bbbbbbbb(?:))(?:bbbbbbbb(?:)(?:bbbbbbbb)))') /* Result: y */;
regexp_like('caaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','(?:c|d)(?:)(?:aaaaaaaa(?:)(?:bbbbbbbb)(?:bbbbbbbb(?:))(?:bbbbbbbb(?:)(?:bbbbbbbb)))')
1
select regexp_like('Ab4ab','(?i)(ab)\\d\\1') /* Result: y */;
regexp_like('Ab4ab','(?i)(ab)\\d\\1')
1
select regexp_like('ab4Ab','(?i)(ab)\\d\\1') /* Result: y */;
regexp_like('ab4Ab','(?i)(ab)\\d\\1')
1
select regexp_like('foobar1234baz','foo\\w*\\d{4}baz') /* Result: y */;
regexp_like('foobar1234baz','foo\\w*\\d{4}baz')
1
select regexp_like('cabd','a(?{})b') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('-','a(?{)b') /* Result: c */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('-','a(?{{})b') /* Result: c */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('-','a(?{}})b') /* Result: c */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('-','a(?{"{"})b') /* Result: c */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('cabd','a(?{"\\{"})b') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('-','a(?{"{"}})b') /* Result: c */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('caxbd','a(?{$bl="\\{"}).b') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('x~~','x(~~)*(?:(?:F)?)?') /* Result: y */;
regexp_like('x~~','x(~~)*(?:(?:F)?)?')
1
select regexp_like('aaac','^a(?#xxx){3}c') /* Result: y */;
regexp_like('aaac','^a(?#xxx){3}c')
1
select regexp_like('aaac','(?x)^a (?#xxx) (?#yyy) {3}c') /* Result: y */;
regexp_like('aaac','(?x)^a (?#xxx) (?#yyy) {3}c')
1
select regexp_like('dbcb','(?<![cd])b') /* Result: n */;
regexp_like('dbcb','(?<![cd])b')
0
select regexp_like('dbaacb','(?<![cd])[ab]') /* Result: y */;
regexp_like('dbaacb','(?<![cd])[ab]')
1
select regexp_like('dbcb','(?<!(c|d))b') /* Result: n */;
regexp_like('dbcb','(?<!(c|d))b')
0
select regexp_like('dbaacb','(?<!(c|d))[ab]') /* Result: y */;
regexp_like('dbaacb','(?<!(c|d))[ab]')
1
select regexp_like('cdaccb','(?<!cd)[ab]') /* Result: y */;
regexp_like('cdaccb','(?<!cd)[ab]')
1
select regexp_like('a--','^(?:a?b?)*$') /* Result: n */;
regexp_like('a--','^(?:a?b?)*$')
0
select regexp_like('a\nb\nc\n','((?s)^a(.))((?m)^b$)') /* Result: y */;
regexp_like('a\nb\nc\n','((?s)^a(.))((?m)^b$)')
1
select regexp_like('a\nb\nc\n','((?m)^b$)') /* Result: y */;
regexp_like('a\nb\nc\n','((?m)^b$)')
1
select regexp_like('a\nb\n','(?m)^b') /* Result: y */;
regexp_like('a\nb\n','(?m)^b')
1
select regexp_like('a\nb\n','(?m)^(b)') /* Result: y */;
regexp_like('a\nb\n','(?m)^(b)')
1
select regexp_like('a\nb\n','((?m)^b)') /* Result: y */;
regexp_like('a\nb\n','((?m)^b)')
1
select regexp_like('a\nb\n','\n((?m)^b)') /* Result: y */;
regexp_like('a\nb\n','\n((?m)^b)')
1
select regexp_like('a\nb\nc\n','((?s).)c(?!.)') /* Result: y */;
regexp_like('a\nb\nc\n','((?s).)c(?!.)')
1
select regexp_like('a\nb\nc\n','((?s)b.)c(?!.)') /* Result: y */;
regexp_like('a\nb\nc\n','((?s)b.)c(?!.)')
1
select regexp_like('a\nb\nc\n','^b') /* Result: n */;
regexp_like('a\nb\nc\n','^b')
0
select regexp_like('a\nb\nc\n','()^b') /* Result: n */;
regexp_like('a\nb\nc\n','()^b')
0
select regexp_like('a\nb\nc\n','((?m)^b)') /* Result: y */;
regexp_like('a\nb\nc\n','((?m)^b)')
1
select regexp_like('a','(?(1)a|b)') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(1)b|a)') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(x)?(?(1)a|b)') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(x)?(?(1)b|a)') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','()?(?(1)b|a)') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','()(?(1)b|a)') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','()?(?(1)a|b)') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('(blah)','^(\\()?blah(?(1)(\\)))$') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('blah','^(\\()?blah(?(1)(\\)))$') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('blah)','^(\\()?blah(?(1)(\\)))$') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('(blah','^(\\()?blah(?(1)(\\)))$') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('(blah)','^(\\(+)?blah(?(1)(\\)))$') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('blah','^(\\(+)?blah(?(1)(\\)))$') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('blah)','^(\\(+)?blah(?(1)(\\)))$') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('(blah','^(\\(+)?blah(?(1)(\\)))$') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(1?)a|b)') /* Result: c */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(1)a|b|c)') /* Result: c */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(?{0})a|b)') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(?{0})b|a)') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(?{1})b|a)') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(?{1})a|b)') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(?!a)a|b)') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(?!a)b|a)') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(?=a)b|a)') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('a','(?(?=a)a|b)') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('aaab','(?=(a+?))(\\1ab)') /* Result: y */;
regexp_like('aaab','(?=(a+?))(\\1ab)')
1
select regexp_like('aaab','^(?=(a+?))\\1ab') /* Result: n */;
regexp_like('aaab','^(?=(a+?))\\1ab')
0
select regexp_like('one:','(\\w+:)+') /* Result: y */;
regexp_like('one:','(\\w+:)+')
1
select regexp_like('a','$(?<=^(a))') /* Result: y */;
regexp_like('a','$(?<=^(a))')
1
select regexp_like('abcd:','([\\w:]+::)?(\\w+)$') /* Result: n */;
regexp_like('abcd:','([\\w:]+::)?(\\w+)$')
0
select regexp_like('abcd','([\\w:]+::)?(\\w+)$') /* Result: y */;
regexp_like('abcd','([\\w:]+::)?(\\w+)$')
1
select regexp_like('xy:z:::abcd','([\\w:]+::)?(\\w+)$') /* Result: y */;
regexp_like('xy:z:::abcd','([\\w:]+::)?(\\w+)$')
1
select regexp_like('aexycd','^[^bcd]*(c+)') /* Result: y */;
regexp_like('aexycd','^[^bcd]*(c+)')
1
select regexp_like('caab','(a*)b+') /* Result: y */;
regexp_like('caab','(a*)b+')
1
select regexp_like('yaaxxaaaacd','(?{$a=2})a*aa(?{local$a=$a+1})k*c(?{$b=$a})') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('yaaxxaaaacd','(?{$a=2})(a(?{local$a=$a+1}))*aak*c(?{$b=$a})') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('aaab','(>a+)ab') /* Result: n */;
regexp_like('aaab','(>a+)ab')
0
select regexp_like('aaab','(?>a+)b') /* Result: y */;
regexp_like('aaab','(?>a+)b')
1
select regexp_like('a:[b]:','([\\[:]+)') /* Result: yi */;
regexp_like('a:[b]:','([\\[:]+)')
1
select regexp_like('a=[b]=','([\\[=]+)') /* Result: yi */;
regexp_like('a=[b]=','([\\[=]+)')
1
select regexp_like('a.[b].','([\\[.]+)') /* Result: yi */;
regexp_like('a.[b].','([\\[.]+)')
1
select regexp_like('-','[a[:xyz:') /* Result: c */;
ERROR HY000: The regular expression contains an unclosed bracket expression.
select regexp_like('-','[a[:xyz:]') /* Result: c */;
ERROR HY000: Illegal argument to a regular expression.
select regexp_like('abc','[a\\[:]b[:c]') /* Result: yi */;
regexp_like('abc','[a\\[:]b[:c]')
1
select regexp_like('pbaq','([a[:xyz:]b]+)') /* Result: c */;
ERROR HY000: Illegal argument to a regular expression.
select regexp_like('abc','[a\\[:]b[:c]') /* Result: iy */;
regexp_like('abc','[a\\[:]b[:c]')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:alpha:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:alpha:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:alnum:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:alnum:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:ascii:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:ascii:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:cntrl:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:cntrl:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:digit:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:digit:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:graph:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:graph:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:lower:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:lower:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:print:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:print:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:punct:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:punct:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:space:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:space:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:word:]]+)') /* Result: yi */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:word:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:upper:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:upper:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:xdigit:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:xdigit:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^alpha:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^alpha:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^alnum:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^alnum:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^ascii:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^ascii:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^cntrl:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^cntrl:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^digit:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^digit:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^lower:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^lower:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^print:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^print:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^punct:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^punct:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^space:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^space:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^word:]]+)') /* Result: yi */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^word:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^upper:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^upper:]]+)')
1
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^xdigit:]]+)') /* Result: y */;
regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^xdigit:]]+)')
1
select regexp_like('-','[[:foo:]]') /* Result: c */;
ERROR HY000: Illegal argument to a regular expression.
select regexp_like('-','[[:^foo:]]') /* Result: c */;
ERROR HY000: Illegal argument to a regular expression.
select regexp_like('aaab','((?>a+)b)') /* Result: y */;
regexp_like('aaab','((?>a+)b)')
1
select regexp_like('aaab','(?>(a+))b') /* Result: y */;
regexp_like('aaab','(?>(a+))b')
1
select regexp_like('((abc(ade)ufh()()x','((?>[^()]+)|\\([^()]*\\))+') /* Result: y */;
regexp_like('((abc(ade)ufh()()x','((?>[^()]+)|\\([^()]*\\))+')
1
select regexp_like('-','(?<=x+)y') /* Result: c */;
ERROR HY000: The look-behind assertion exceeds the limit in regular expression.
select regexp_like('-','a{37,17}') /* Result: c */;
ERROR HY000: The maximum is less than the minumum in a {min,max} interval.
select regexp_like('a\nb\n','\\Z') /* Result: y */;
regexp_like('a\nb\n','\\Z')
1
select regexp_like('a\nb\n','\\z') /* Result: y */;
regexp_like('a\nb\n','\\z')
1
select regexp_like('a\nb\n','$') /* Result: y */;
regexp_like('a\nb\n','$')
1
select regexp_like('b\na\n','\\Z') /* Result: y */;
regexp_like('b\na\n','\\Z')
1
select regexp_like('b\na\n','\\z') /* Result: y */;
regexp_like('b\na\n','\\z')
1
select regexp_like('b\na\n','$') /* Result: y */;
regexp_like('b\na\n','$')
1
select regexp_like('b\na','\\Z') /* Result: y */;
regexp_like('b\na','\\Z')
1
select regexp_like('b\na','\\z') /* Result: y */;
regexp_like('b\na','\\z')
1
select regexp_like('b\na','$') /* Result: y */;
regexp_like('b\na','$')
1
select regexp_like('a\nb\n','(?m)\\Z') /* Result: y */;
regexp_like('a\nb\n','(?m)\\Z')
1
select regexp_like('a\nb\n','(?m)\\z') /* Result: y */;
regexp_like('a\nb\n','(?m)\\z')
1
select regexp_like('a\nb\n','(?m)$') /* Result: y */;
regexp_like('a\nb\n','(?m)$')
1
select regexp_like('b\na\n','(?m)\\Z') /* Result: y */;
regexp_like('b\na\n','(?m)\\Z')
1
select regexp_like('b\na\n','(?m)\\z') /* Result: y */;
regexp_like('b\na\n','(?m)\\z')
1
select regexp_like('b\na\n','(?m)$') /* Result: y */;
regexp_like('b\na\n','(?m)$')
1
select regexp_like('b\na','(?m)\\Z') /* Result: y */;
regexp_like('b\na','(?m)\\Z')
1
select regexp_like('b\na','(?m)\\z') /* Result: y */;
regexp_like('b\na','(?m)\\z')
1
select regexp_like('b\na','(?m)$') /* Result: y */;
regexp_like('b\na','(?m)$')
1
select regexp_like('a\nb\n','a\\Z') /* Result: n */;
regexp_like('a\nb\n','a\\Z')
0
select regexp_like('a\nb\n','a\\z') /* Result: n */;
regexp_like('a\nb\n','a\\z')
0
select regexp_like('a\nb\n','a$') /* Result: n */;
regexp_like('a\nb\n','a$')
0
select regexp_like('b\na\n','a\\Z') /* Result: y */;
regexp_like('b\na\n','a\\Z')
1
select regexp_like('b\na\n','a\\z') /* Result: n */;
regexp_like('b\na\n','a\\z')
0
select regexp_like('b\na\n','a$') /* Result: y */;
regexp_like('b\na\n','a$')
1
select regexp_like('b\na','a\\Z') /* Result: y */;
regexp_like('b\na','a\\Z')
1
select regexp_like('b\na','a\\z') /* Result: y */;
regexp_like('b\na','a\\z')
1
select regexp_like('b\na','a$') /* Result: y */;
regexp_like('b\na','a$')
1
select regexp_like('a\nb\n','(?m)a\\Z') /* Result: n */;
regexp_like('a\nb\n','(?m)a\\Z')
0
select regexp_like('a\nb\n','(?m)a\\z') /* Result: n */;
regexp_like('a\nb\n','(?m)a\\z')
0
select regexp_like('a\nb\n','(?m)a$') /* Result: y */;
regexp_like('a\nb\n','(?m)a$')
1
select regexp_like('b\na\n','(?m)a\\Z') /* Result: y */;
regexp_like('b\na\n','(?m)a\\Z')
1
select regexp_like('b\na\n','(?m)a\\z') /* Result: n */;
regexp_like('b\na\n','(?m)a\\z')
0
select regexp_like('b\na\n','(?m)a$') /* Result: y */;
regexp_like('b\na\n','(?m)a$')
1
select regexp_like('b\na','(?m)a\\Z') /* Result: y */;
regexp_like('b\na','(?m)a\\Z')
1
select regexp_like('b\na','(?m)a\\z') /* Result: y */;
regexp_like('b\na','(?m)a\\z')
1
select regexp_like('b\na','(?m)a$') /* Result: y */;
regexp_like('b\na','(?m)a$')
1
select regexp_like('aa\nb\n','aa\\Z') /* Result: n */;
regexp_like('aa\nb\n','aa\\Z')
0
select regexp_like('aa\nb\n','aa\\z') /* Result: n */;
regexp_like('aa\nb\n','aa\\z')
0
select regexp_like('aa\nb\n','aa$') /* Result: n */;
regexp_like('aa\nb\n','aa$')
0
select regexp_like('b\naa\n','aa\\Z') /* Result: y */;
regexp_like('b\naa\n','aa\\Z')
1
select regexp_like('b\naa\n','aa\\z') /* Result: n */;
regexp_like('b\naa\n','aa\\z')
0
select regexp_like('b\naa\n','aa$') /* Result: y */;
regexp_like('b\naa\n','aa$')
1
select regexp_like('b\naa','aa\\Z') /* Result: y */;
regexp_like('b\naa','aa\\Z')
1
select regexp_like('b\naa','aa\\z') /* Result: y */;
regexp_like('b\naa','aa\\z')
1
select regexp_like('b\naa','aa$') /* Result: y */;
regexp_like('b\naa','aa$')
1
select regexp_like('aa\nb\n','(?m)aa\\Z') /* Result: n */;
regexp_like('aa\nb\n','(?m)aa\\Z')
0
select regexp_like('aa\nb\n','(?m)aa\\z') /* Result: n */;
regexp_like('aa\nb\n','(?m)aa\\z')
0
select regexp_like('aa\nb\n','(?m)aa$') /* Result: y */;
regexp_like('aa\nb\n','(?m)aa$')
1
select regexp_like('b\naa\n','(?m)aa\\Z') /* Result: y */;
regexp_like('b\naa\n','(?m)aa\\Z')
1
select regexp_like('b\naa\n','(?m)aa\\z') /* Result: n */;
regexp_like('b\naa\n','(?m)aa\\z')
0
select regexp_like('b\naa\n','(?m)aa$') /* Result: y */;
regexp_like('b\naa\n','(?m)aa$')
1
select regexp_like('b\naa','(?m)aa\\Z') /* Result: y */;
regexp_like('b\naa','(?m)aa\\Z')
1
select regexp_like('b\naa','(?m)aa\\z') /* Result: y */;
regexp_like('b\naa','(?m)aa\\z')
1
select regexp_like('b\naa','(?m)aa$') /* Result: y */;
regexp_like('b\naa','(?m)aa$')
1
select regexp_like('ac\nb\n','aa\\Z') /* Result: n */;
regexp_like('ac\nb\n','aa\\Z')
0
select regexp_like('ac\nb\n','aa\\z') /* Result: n */;
regexp_like('ac\nb\n','aa\\z')
0
select regexp_like('ac\nb\n','aa$') /* Result: n */;
regexp_like('ac\nb\n','aa$')
0
select regexp_like('b\nac\n','aa\\Z') /* Result: n */;
regexp_like('b\nac\n','aa\\Z')
0
select regexp_like('b\nac\n','aa\\z') /* Result: n */;
regexp_like('b\nac\n','aa\\z')
0
select regexp_like('b\nac\n','aa$') /* Result: n */;
regexp_like('b\nac\n','aa$')
0
select regexp_like('b\nac','aa\\Z') /* Result: n */;
regexp_like('b\nac','aa\\Z')
0
select regexp_like('b\nac','aa\\z') /* Result: n */;
regexp_like('b\nac','aa\\z')
0
select regexp_like('b\nac','aa$') /* Result: n */;
regexp_like('b\nac','aa$')
0
select regexp_like('ac\nb\n','(?m)aa\\Z') /* Result: n */;
regexp_like('ac\nb\n','(?m)aa\\Z')
0
select regexp_like('ac\nb\n','(?m)aa\\z') /* Result: n */;
regexp_like('ac\nb\n','(?m)aa\\z')
0
select regexp_like('ac\nb\n','(?m)aa$') /* Result: n */;
regexp_like('ac\nb\n','(?m)aa$')
0
select regexp_like('b\nac\n','(?m)aa\\Z') /* Result: n */;
regexp_like('b\nac\n','(?m)aa\\Z')
0
select regexp_like('b\nac\n','(?m)aa\\z') /* Result: n */;
regexp_like('b\nac\n','(?m)aa\\z')
0
select regexp_like('b\nac\n','(?m)aa$') /* Result: n */;
regexp_like('b\nac\n','(?m)aa$')
0
select regexp_like('b\nac','(?m)aa\\Z') /* Result: n */;
regexp_like('b\nac','(?m)aa\\Z')
0
select regexp_like('b\nac','(?m)aa\\z') /* Result: n */;
regexp_like('b\nac','(?m)aa\\z')
0
select regexp_like('b\nac','(?m)aa$') /* Result: n */;
regexp_like('b\nac','(?m)aa$')
0
select regexp_like('ca\nb\n','aa\\Z') /* Result: n */;
regexp_like('ca\nb\n','aa\\Z')
0
select regexp_like('ca\nb\n','aa\\z') /* Result: n */;
regexp_like('ca\nb\n','aa\\z')
0
select regexp_like('ca\nb\n','aa$') /* Result: n */;
regexp_like('ca\nb\n','aa$')
0
select regexp_like('b\nca\n','aa\\Z') /* Result: n */;
regexp_like('b\nca\n','aa\\Z')
0
select regexp_like('b\nca\n','aa\\z') /* Result: n */;
regexp_like('b\nca\n','aa\\z')
0
select regexp_like('b\nca\n','aa$') /* Result: n */;
regexp_like('b\nca\n','aa$')
0
select regexp_like('b\nca','aa\\Z') /* Result: n */;
regexp_like('b\nca','aa\\Z')
0
select regexp_like('b\nca','aa\\z') /* Result: n */;
regexp_like('b\nca','aa\\z')
0
select regexp_like('b\nca','aa$') /* Result: n */;
regexp_like('b\nca','aa$')
0
select regexp_like('ca\nb\n','(?m)aa\\Z') /* Result: n */;
regexp_like('ca\nb\n','(?m)aa\\Z')
0
select regexp_like('ca\nb\n','(?m)aa\\z') /* Result: n */;
regexp_like('ca\nb\n','(?m)aa\\z')
0
select regexp_like('ca\nb\n','(?m)aa$') /* Result: n */;
regexp_like('ca\nb\n','(?m)aa$')
0
select regexp_like('b\nca\n','(?m)aa\\Z') /* Result: n */;
regexp_like('b\nca\n','(?m)aa\\Z')
0
select regexp_like('b\nca\n','(?m)aa\\z') /* Result: n */;
regexp_like('b\nca\n','(?m)aa\\z')
0
select regexp_like('b\nca\n','(?m)aa$') /* Result: n */;
regexp_like('b\nca\n','(?m)aa$')
0
select regexp_like('b\nca','(?m)aa\\Z') /* Result: n */;
regexp_like('b\nca','(?m)aa\\Z')
0
select regexp_like('b\nca','(?m)aa\\z') /* Result: n */;
regexp_like('b\nca','(?m)aa\\z')
0
select regexp_like('b\nca','(?m)aa$') /* Result: n */;
regexp_like('b\nca','(?m)aa$')
0
select regexp_like('ab\nb\n','ab\\Z') /* Result: n */;
regexp_like('ab\nb\n','ab\\Z')
0
select regexp_like('ab\nb\n','ab\\z') /* Result: n */;
regexp_like('ab\nb\n','ab\\z')
0
select regexp_like('ab\nb\n','ab$') /* Result: n */;
regexp_like('ab\nb\n','ab$')
0
select regexp_like('b\nab\n','ab\\Z') /* Result: y */;
regexp_like('b\nab\n','ab\\Z')
1
select regexp_like('b\nab\n','ab\\z') /* Result: n */;
regexp_like('b\nab\n','ab\\z')
0
select regexp_like('b\nab\n','ab$') /* Result: y */;
regexp_like('b\nab\n','ab$')
1
select regexp_like('b\nab','ab\\Z') /* Result: y */;
regexp_like('b\nab','ab\\Z')
1
select regexp_like('b\nab','ab\\z') /* Result: y */;
regexp_like('b\nab','ab\\z')
1
select regexp_like('b\nab','ab$') /* Result: y */;
regexp_like('b\nab','ab$')
1
select regexp_like('ab\nb\n','(?m)ab\\Z') /* Result: n */;
regexp_like('ab\nb\n','(?m)ab\\Z')
0
select regexp_like('ab\nb\n','(?m)ab\\z') /* Result: n */;
regexp_like('ab\nb\n','(?m)ab\\z')
0
select regexp_like('ab\nb\n','(?m)ab$') /* Result: y */;
regexp_like('ab\nb\n','(?m)ab$')
1
select regexp_like('b\nab\n','(?m)ab\\Z') /* Result: y */;
regexp_like('b\nab\n','(?m)ab\\Z')
1
select regexp_like('b\nab\n','(?m)ab\\z') /* Result: n */;
regexp_like('b\nab\n','(?m)ab\\z')
0
select regexp_like('b\nab\n','(?m)ab$') /* Result: y */;
regexp_like('b\nab\n','(?m)ab$')
1
select regexp_like('b\nab','(?m)ab\\Z') /* Result: y */;
regexp_like('b\nab','(?m)ab\\Z')
1
select regexp_like('b\nab','(?m)ab\\z') /* Result: y */;
regexp_like('b\nab','(?m)ab\\z')
1
select regexp_like('b\nab','(?m)ab$') /* Result: y */;
regexp_like('b\nab','(?m)ab$')
1
select regexp_like('ac\nb\n','ab\\Z') /* Result: n */;
regexp_like('ac\nb\n','ab\\Z')
0
select regexp_like('ac\nb\n','ab\\z') /* Result: n */;
regexp_like('ac\nb\n','ab\\z')
0
select regexp_like('ac\nb\n','ab$') /* Result: n */;
regexp_like('ac\nb\n','ab$')
0
select regexp_like('b\nac\n','ab\\Z') /* Result: n */;
regexp_like('b\nac\n','ab\\Z')
0
select regexp_like('b\nac\n','ab\\z') /* Result: n */;
regexp_like('b\nac\n','ab\\z')
0
select regexp_like('b\nac\n','ab$') /* Result: n */;
regexp_like('b\nac\n','ab$')
0
select regexp_like('b\nac','ab\\Z') /* Result: n */;
regexp_like('b\nac','ab\\Z')
0
select regexp_like('b\nac','ab\\z') /* Result: n */;
regexp_like('b\nac','ab\\z')
0
select regexp_like('b\nac','ab$') /* Result: n */;
regexp_like('b\nac','ab$')
0
select regexp_like('ac\nb\n','(?m)ab\\Z') /* Result: n */;
regexp_like('ac\nb\n','(?m)ab\\Z')
0
select regexp_like('ac\nb\n','(?m)ab\\z') /* Result: n */;
regexp_like('ac\nb\n','(?m)ab\\z')
0
select regexp_like('ac\nb\n','(?m)ab$') /* Result: n */;
regexp_like('ac\nb\n','(?m)ab$')
0
select regexp_like('b\nac\n','(?m)ab\\Z') /* Result: n */;
regexp_like('b\nac\n','(?m)ab\\Z')
0
select regexp_like('b\nac\n','(?m)ab\\z') /* Result: n */;
regexp_like('b\nac\n','(?m)ab\\z')
0
select regexp_like('b\nac\n','(?m)ab$') /* Result: n */;
regexp_like('b\nac\n','(?m)ab$')
0
select regexp_like('b\nac','(?m)ab\\Z') /* Result: n */;
regexp_like('b\nac','(?m)ab\\Z')
0
select regexp_like('b\nac','(?m)ab\\z') /* Result: n */;
regexp_like('b\nac','(?m)ab\\z')
0
select regexp_like('b\nac','(?m)ab$') /* Result: n */;
regexp_like('b\nac','(?m)ab$')
0
select regexp_like('ca\nb\n','ab\\Z') /* Result: n */;
regexp_like('ca\nb\n','ab\\Z')
0
select regexp_like('ca\nb\n','ab\\z') /* Result: n */;
regexp_like('ca\nb\n','ab\\z')
0
select regexp_like('ca\nb\n','ab$') /* Result: n */;
regexp_like('ca\nb\n','ab$')
0
select regexp_like('b\nca\n','ab\\Z') /* Result: n */;
regexp_like('b\nca\n','ab\\Z')
0
select regexp_like('b\nca\n','ab\\z') /* Result: n */;
regexp_like('b\nca\n','ab\\z')
0
select regexp_like('b\nca\n','ab$') /* Result: n */;
regexp_like('b\nca\n','ab$')
0
select regexp_like('b\nca','ab\\Z') /* Result: n */;
regexp_like('b\nca','ab\\Z')
0
select regexp_like('b\nca','ab\\z') /* Result: n */;
regexp_like('b\nca','ab\\z')
0
select regexp_like('b\nca','ab$') /* Result: n */;
regexp_like('b\nca','ab$')
0
select regexp_like('ca\nb\n','(?m)ab\\Z') /* Result: n */;
regexp_like('ca\nb\n','(?m)ab\\Z')
0
select regexp_like('ca\nb\n','(?m)ab\\z') /* Result: n */;
regexp_like('ca\nb\n','(?m)ab\\z')
0
select regexp_like('ca\nb\n','(?m)ab$') /* Result: n */;
regexp_like('ca\nb\n','(?m)ab$')
0
select regexp_like('b\nca\n','(?m)ab\\Z') /* Result: n */;
regexp_like('b\nca\n','(?m)ab\\Z')
0
select regexp_like('b\nca\n','(?m)ab\\z') /* Result: n */;
regexp_like('b\nca\n','(?m)ab\\z')
0
select regexp_like('b\nca\n','(?m)ab$') /* Result: n */;
regexp_like('b\nca\n','(?m)ab$')
0
select regexp_like('b\nca','(?m)ab\\Z') /* Result: n */;
regexp_like('b\nca','(?m)ab\\Z')
0
select regexp_like('b\nca','(?m)ab\\z') /* Result: n */;
regexp_like('b\nca','(?m)ab\\z')
0
select regexp_like('b\nca','(?m)ab$') /* Result: n */;
regexp_like('b\nca','(?m)ab$')
0
select regexp_like('abb\nb\n','abb\\Z') /* Result: n */;
regexp_like('abb\nb\n','abb\\Z')
0
select regexp_like('abb\nb\n','abb\\z') /* Result: n */;
regexp_like('abb\nb\n','abb\\z')
0
select regexp_like('abb\nb\n','abb$') /* Result: n */;
regexp_like('abb\nb\n','abb$')
0
select regexp_like('b\nabb\n','abb\\Z') /* Result: y */;
regexp_like('b\nabb\n','abb\\Z')
1
select regexp_like('b\nabb\n','abb\\z') /* Result: n */;
regexp_like('b\nabb\n','abb\\z')
0
select regexp_like('b\nabb\n','abb$') /* Result: y */;
regexp_like('b\nabb\n','abb$')
1
select regexp_like('b\nabb','abb\\Z') /* Result: y */;
regexp_like('b\nabb','abb\\Z')
1
select regexp_like('b\nabb','abb\\z') /* Result: y */;
regexp_like('b\nabb','abb\\z')
1
select regexp_like('b\nabb','abb$') /* Result: y */;
regexp_like('b\nabb','abb$')
1
select regexp_like('abb\nb\n','(?m)abb\\Z') /* Result: n */;
regexp_like('abb\nb\n','(?m)abb\\Z')
0
select regexp_like('abb\nb\n','(?m)abb\\z') /* Result: n */;
regexp_like('abb\nb\n','(?m)abb\\z')
0
select regexp_like('abb\nb\n','(?m)abb$') /* Result: y */;
regexp_like('abb\nb\n','(?m)abb$')
1
select regexp_like('b\nabb\n','(?m)abb\\Z') /* Result: y */;
regexp_like('b\nabb\n','(?m)abb\\Z')
1
select regexp_like('b\nabb\n','(?m)abb\\z') /* Result: n */;
regexp_like('b\nabb\n','(?m)abb\\z')
0
select regexp_like('b\nabb\n','(?m)abb$') /* Result: y */;
regexp_like('b\nabb\n','(?m)abb$')
1
select regexp_like('b\nabb','(?m)abb\\Z') /* Result: y */;
regexp_like('b\nabb','(?m)abb\\Z')
1
select regexp_like('b\nabb','(?m)abb\\z') /* Result: y */;
regexp_like('b\nabb','(?m)abb\\z')
1
select regexp_like('b\nabb','(?m)abb$') /* Result: y */;
regexp_like('b\nabb','(?m)abb$')
1
select regexp_like('ac\nb\n','abb\\Z') /* Result: n */;
regexp_like('ac\nb\n','abb\\Z')
0
select regexp_like('ac\nb\n','abb\\z') /* Result: n */;
regexp_like('ac\nb\n','abb\\z')
0
select regexp_like('ac\nb\n','abb$') /* Result: n */;
regexp_like('ac\nb\n','abb$')
0
select regexp_like('b\nac\n','abb\\Z') /* Result: n */;
regexp_like('b\nac\n','abb\\Z')
0
select regexp_like('b\nac\n','abb\\z') /* Result: n */;
regexp_like('b\nac\n','abb\\z')
0
select regexp_like('b\nac\n','abb$') /* Result: n */;
regexp_like('b\nac\n','abb$')
0
select regexp_like('b\nac','abb\\Z') /* Result: n */;
regexp_like('b\nac','abb\\Z')
0
select regexp_like('b\nac','abb\\z') /* Result: n */;
regexp_like('b\nac','abb\\z')
0
select regexp_like('b\nac','abb$') /* Result: n */;
regexp_like('b\nac','abb$')
0
select regexp_like('ac\nb\n','(?m)abb\\Z') /* Result: n */;
regexp_like('ac\nb\n','(?m)abb\\Z')
0
select regexp_like('ac\nb\n','(?m)abb\\z') /* Result: n */;
regexp_like('ac\nb\n','(?m)abb\\z')
0
select regexp_like('ac\nb\n','(?m)abb$') /* Result: n */;
regexp_like('ac\nb\n','(?m)abb$')
0
select regexp_like('b\nac\n','(?m)abb\\Z') /* Result: n */;
regexp_like('b\nac\n','(?m)abb\\Z')
0
select regexp_like('b\nac\n','(?m)abb\\z') /* Result: n */;
regexp_like('b\nac\n','(?m)abb\\z')
0
select regexp_like('b\nac\n','(?m)abb$') /* Result: n */;
regexp_like('b\nac\n','(?m)abb$')
0
select regexp_like('b\nac','(?m)abb\\Z') /* Result: n */;
regexp_like('b\nac','(?m)abb\\Z')
0
select regexp_like('b\nac','(?m)abb\\z') /* Result: n */;
regexp_like('b\nac','(?m)abb\\z')
0
select regexp_like('b\nac','(?m)abb$') /* Result: n */;
regexp_like('b\nac','(?m)abb$')
0
select regexp_like('ca\nb\n','abb\\Z') /* Result: n */;
regexp_like('ca\nb\n','abb\\Z')
0
select regexp_like('ca\nb\n','abb\\z') /* Result: n */;
regexp_like('ca\nb\n','abb\\z')
0
select regexp_like('ca\nb\n','abb$') /* Result: n */;
regexp_like('ca\nb\n','abb$')
0
select regexp_like('b\nca\n','abb\\Z') /* Result: n */;
regexp_like('b\nca\n','abb\\Z')
0
select regexp_like('b\nca\n','abb\\z') /* Result: n */;
regexp_like('b\nca\n','abb\\z')
0
select regexp_like('b\nca\n','abb$') /* Result: n */;
regexp_like('b\nca\n','abb$')
0
select regexp_like('b\nca','abb\\Z') /* Result: n */;
regexp_like('b\nca','abb\\Z')
0
select regexp_like('b\nca','abb\\z') /* Result: n */;
regexp_like('b\nca','abb\\z')
0
select regexp_like('b\nca','abb$') /* Result: n */;
regexp_like('b\nca','abb$')
0
select regexp_like('ca\nb\n','(?m)abb\\Z') /* Result: n */;
regexp_like('ca\nb\n','(?m)abb\\Z')
0
select regexp_like('ca\nb\n','(?m)abb\\z') /* Result: n */;
regexp_like('ca\nb\n','(?m)abb\\z')
0
select regexp_like('ca\nb\n','(?m)abb$') /* Result: n */;
regexp_like('ca\nb\n','(?m)abb$')
0
select regexp_like('b\nca\n','(?m)abb\\Z') /* Result: n */;
regexp_like('b\nca\n','(?m)abb\\Z')
0
select regexp_like('b\nca\n','(?m)abb\\z') /* Result: n */;
regexp_like('b\nca\n','(?m)abb\\z')
0
select regexp_like('b\nca\n','(?m)abb$') /* Result: n */;
regexp_like('b\nca\n','(?m)abb$')
0
select regexp_like('b\nca','(?m)abb\\Z') /* Result: n */;
regexp_like('b\nca','(?m)abb\\Z')
0
select regexp_like('b\nca','(?m)abb\\z') /* Result: n */;
regexp_like('b\nca','(?m)abb\\z')
0
select regexp_like('b\nca','(?m)abb$') /* Result: n */;
regexp_like('b\nca','(?m)abb$')
0
select regexp_like('ca','(^|x)(c)') /* Result: y */;
regexp_like('ca','(^|x)(c)')
1
select regexp_like('x','a*abc?xyz+pqr{3}ab{2,}xy{4,5}pq{0,6}AB{0,}zz') /* Result: n */;
regexp_like('x','a*abc?xyz+pqr{3}ab{2,}xy{4,5}pq{0,6}AB{0,}zz')
0
select regexp_like('yabz','a(?{$a=2;$b=3;($b)=$a})b') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('_I(round(xs * sz),1)','round\\(((?>[^()]+))\\)') /* Result: y */;
regexp_like('_I(round(xs * sz),1)','round\\(((?>[^()]+))\\)')
1
select regexp_like('x ','(?x)((?x:.) )') /* Result: y */;
regexp_like('x ','(?x)((?x:.) )')
1
select regexp_like('x ','(?x)((?-x:.) )') /* Result: y */;
regexp_like('x ','(?x)((?-x:.) )')
1
select regexp_like('foo.bart','foo.bart') /* Result: y */;
regexp_like('foo.bart','foo.bart')
1
select regexp_like('abcd\ndxxx','(?m)^d[x][x][x]') /* Result: y */;
regexp_like('abcd\ndxxx','(?m)^d[x][x][x]')
1
select regexp_like('xxxtt','tt+$') /* Result: y */;
regexp_like('xxxtt','tt+$')
1
select regexp_like('za-9z','([a\\-\\d]+)') /* Result: yi */;
regexp_like('za-9z','([a\\-\\d]+)')
1
select regexp_like('a0-za','([\\d-z]+)') /* Result: y */;
regexp_like('a0-za','([\\d-z]+)')
1
select regexp_like('a0- z','([\\d-\\s]+)') /* Result: y */;
regexp_like('a0- z','([\\d-\\s]+)')
1
select regexp_like('za-9z','([a-[:digit:]]+)') /* Result: y */;
regexp_like('za-9z','([a-[:digit:]]+)')
1
select regexp_like('=0-z=','([[:digit:]-z]+)') /* Result: y */;
regexp_like('=0-z=','([[:digit:]-z]+)')
1
select regexp_like('=0-z=','([[:digit:]-[:alpha:]]+)') /* Result: iy */;
regexp_like('=0-z=','([[:digit:]-[:alpha:]]+)')
1
select regexp_like('aaaXbX','\\GX.*X') /* Result: n */;
regexp_like('aaaXbX','\\GX.*X')
0
select regexp_like('3.1415926','(\\d+\\.\\d+)') /* Result: y */;
regexp_like('3.1415926','(\\d+\\.\\d+)')
1
select regexp_like('have a web browser','(\\ba.{0,10}br)') /* Result: y */;
regexp_like('have a web browser','(\\ba.{0,10}br)')
1
select regexp_like('Changes','(?i)\\.c(pp|xx|c)?$') /* Result: n */;
regexp_like('Changes','(?i)\\.c(pp|xx|c)?$')
0
select regexp_like('IO.c','(?i)\\.c(pp|xx|c)?$') /* Result: y */;
regexp_like('IO.c','(?i)\\.c(pp|xx|c)?$')
1
select regexp_like('IO.c','(?i)(\\.c(pp|xx|c)?$)') /* Result: y */;
regexp_like('IO.c','(?i)(\\.c(pp|xx|c)?$)')
1
select regexp_like('C:/','^([a-z]:)') /* Result: n */;
regexp_like('C:/','^([a-z]:)')
1
select regexp_like('\nx aa','(?m)^\\S\\s+aa$') /* Result: y */;
regexp_like('\nx aa','(?m)^\\S\\s+aa$')
1
select regexp_like('ab','(^|a)b') /* Result: y */;
regexp_like('ab','(^|a)b')
1
select regexp_like('abac','^([ab]*?)(b)?(c)$') /* Result: y */;
regexp_like('abac','^([ab]*?)(b)?(c)$')
1
select regexp_like('abcab','(\\w)?(abc)\\1b') /* Result: n */;
regexp_like('abcab','(\\w)?(abc)\\1b')
0
select regexp_like('a,b,c','^(?:.,){2}c') /* Result: y */;
regexp_like('a,b,c','^(?:.,){2}c')
1
select regexp_like('a,b,c','^(.,){2}c') /* Result: y */;
regexp_like('a,b,c','^(.,){2}c')
1
select regexp_like('a,b,c','^(?:[^,]*,){2}c') /* Result: y */;
regexp_like('a,b,c','^(?:[^,]*,){2}c')
1
select regexp_like('a,b,c','^([^,]*,){2}c') /* Result: y */;
regexp_like('a,b,c','^([^,]*,){2}c')
1
select regexp_like('aaa,b,c,d','^([^,]*,){3}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]*,){3}d')
1
select regexp_like('aaa,b,c,d','^([^,]*,){3,}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]*,){3,}d')
1
select regexp_like('aaa,b,c,d','^([^,]*,){0,3}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]*,){0,3}d')
1
select regexp_like('aaa,b,c,d','^([^,]{1,3},){3}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{1,3},){3}d')
1
select regexp_like('aaa,b,c,d','^([^,]{1,3},){3,}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{1,3},){3,}d')
1
select regexp_like('aaa,b,c,d','^([^,]{1,3},){0,3}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{1,3},){0,3}d')
1
select regexp_like('aaa,b,c,d','^([^,]{1,},){3}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{1,},){3}d')
1
select regexp_like('aaa,b,c,d','^([^,]{1,},){3,}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{1,},){3,}d')
1
select regexp_like('aaa,b,c,d','^([^,]{1,},){0,3}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{1,},){0,3}d')
1
select regexp_like('aaa,b,c,d','^([^,]{0,3},){3}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{0,3},){3}d')
1
select regexp_like('aaa,b,c,d','^([^,]{0,3},){3,}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{0,3},){3,}d')
1
select regexp_like('aaa,b,c,d','^([^,]{0,3},){0,3}d') /* Result: y */;
regexp_like('aaa,b,c,d','^([^,]{0,3},){0,3}d')
1
select regexp_like('','(?i)') /* Result: y */;
regexp_like('','(?i)')
1
select regexp_like('a\nxb\n','(?m)(?!\\A)x') /* Result: y */;
regexp_like('a\nxb\n','(?m)(?!\\A)x')
1
select regexp_like('aba','^(a(b)?)+$') /* Result: yi */;
regexp_like('aba','^(a(b)?)+$')
1
select regexp_like('123\nabcabcabcabc\n','(?m)^.{9}abc.*\n') /* Result: y */;
regexp_like('123\nabcabcabcabc\n','(?m)^.{9}abc.*\n')
1
select regexp_like('a','^(a)?a$') /* Result: y */;
regexp_like('a','^(a)?a$')
1
select regexp_like('a','^(a)?(?(1)a|b)+$') /* Result: n */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('aaaaaa','^(a\\1?)(a\\1?)(a\\2?)(a\\3?)$') /* Result: y */;
regexp_like('aaaaaa','^(a\\1?)(a\\1?)(a\\2?)(a\\3?)$')
1
select regexp_like('aaaaaa','^(a\\1?){4}$') /* Result: y */;
regexp_like('aaaaaa','^(a\\1?){4}$')
1
select regexp_like('x1','^(0+)?(?:x(1))?') /* Result: y */;
regexp_like('x1','^(0+)?(?:x(1))?')
1
select regexp_like('012cxx0190','^([0-9a-fA-F]+)(?:x([0-9a-fA-F]+)?)(?:x([0-9a-fA-F]+))?') /* Result: y */;
regexp_like('012cxx0190','^([0-9a-fA-F]+)(?:x([0-9a-fA-F]+)?)(?:x([0-9a-fA-F]+))?')
1
select regexp_like('bbbac','^(b+?|a){1,2}c') /* Result: y */;
regexp_like('bbbac','^(b+?|a){1,2}c')
1
select regexp_like('bbbbac','^(b+?|a){1,2}c') /* Result: y */;
regexp_like('bbbbac','^(b+?|a){1,2}c')
1
select regexp_like('cd. (A. Tw)','\\((\\w\\. \\w+)\\)') /* Result: y */;
regexp_like('cd. (A. Tw)','\\((\\w\\. \\w+)\\)')
1
select regexp_like('aaaacccc','((?:aaaa|bbbb)cccc)?') /* Result: y */;
regexp_like('aaaacccc','((?:aaaa|bbbb)cccc)?')
1
select regexp_like('bbbbcccc','((?:aaaa|bbbb)cccc)?') /* Result: y */;
regexp_like('bbbbcccc','((?:aaaa|bbbb)cccc)?')
1
select regexp_like('a','(a)?(a)+') /* Result: y */;
regexp_like('a','(a)?(a)+')
1
select regexp_like('ab','(ab)?(ab)+') /* Result: y */;
regexp_like('ab','(ab)?(ab)+')
1
select regexp_like('abc','(abc)?(abc)+') /* Result: y */;
regexp_like('abc','(abc)?(abc)+')
1
select regexp_like('a\nb\n','(?m)b\\s^') /* Result: n */;
regexp_like('a\nb\n','(?m)b\\s^')
0
select regexp_like('a','\\ba') /* Result: y */;
regexp_like('a','\\ba')
1
select regexp_like('ab','^(a(??{"(?!)"})|(a)(?{1}))b') /* Result: yi */;
ERROR HY000: Syntax error in regular expression on line 1, character 6.
select regexp_like('AbCd','ab(?i)cd') /* Result: n */;
regexp_like('AbCd','ab(?i)cd')
1
select regexp_like('abCd','ab(?i)cd') /* Result: y */;
regexp_like('abCd','ab(?i)cd')
1
select regexp_like('CD','(A|B)*(?(1)(CD)|(CD))') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('ABCD','(A|B)*(?(1)(CD)|(CD))') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('CD','(A|B)*?(?(1)(CD)|(CD))') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('ABCD','(A|B)*?(?(1)(CD)|(CD))') /* Result: y */;
ERROR HY000: The regular expression contains a feature that is not implemented in this library version.
select regexp_like('Oo','(?i)^(o)(?!.*\\1)') /* Result: n */;
regexp_like('Oo','(?i)^(o)(?!.*\\1)')
0
select regexp_like('abc12bc','(.*)\\d+\\1') /* Result: y */;
regexp_like('abc12bc','(.*)\\d+\\1')
1
select regexp_like('foo\n bar','(?m:(foo\\s*$))') /* Result: y */;
regexp_like('foo\n bar','(?m:(foo\\s*$))')
1
select regexp_like('abcd','(.*)c') /* Result: y */;
regexp_like('abcd','(.*)c')
1
select regexp_like('abcd','(.*)(?=c)') /* Result: y */;
regexp_like('abcd','(.*)(?=c)')
1
select regexp_like('abcd','(.*)(?=c)c') /* Result: yB */;
regexp_like('abcd','(.*)(?=c)c')
1
select regexp_like('abcd','(.*)(?=b|c)') /* Result: y */;
regexp_like('abcd','(.*)(?=b|c)')
1
select regexp_like('abcd','(.*)(?=b|c)c') /* Result: y */;
regexp_like('abcd','(.*)(?=b|c)c')
1
select regexp_like('abcd','(.*)(?=c|b)') /* Result: y */;
regexp_like('abcd','(.*)(?=c|b)')
1
select regexp_like('abcd','(.*)(?=c|b)c') /* Result: y */;
regexp_like('abcd','(.*)(?=c|b)c')
1
select regexp_like('abcd','(.*)(?=[bc])') /* Result: y */;
regexp_like('abcd','(.*)(?=[bc])')
1
select regexp_like('abcd','(.*)(?=[bc])c') /* Result: yB */;
regexp_like('abcd','(.*)(?=[bc])c')
1
select regexp_like('abcd','(.*)(?<=b)') /* Result: y */;
regexp_like('abcd','(.*)(?<=b)')
1
select regexp_like('abcd','(.*)(?<=b)c') /* Result: y */;
regexp_like('abcd','(.*)(?<=b)c')
1
select regexp_like('abcd','(.*)(?<=b|c)') /* Result: y */;
regexp_like('abcd','(.*)(?<=b|c)')
1
select regexp_like('abcd','(.*)(?<=b|c)c') /* Result: y */;
regexp_like('abcd','(.*)(?<=b|c)c')
1
select regexp_like('abcd','(.*)(?<=c|b)') /* Result: y */;
regexp_like('abcd','(.*)(?<=c|b)')
1
select regexp_like('abcd','(.*)(?<=c|b)c') /* Result: y */;
regexp_like('abcd','(.*)(?<=c|b)c')
1
select regexp_like('abcd','(.*)(?<=[bc])') /* Result: y */;
regexp_like('abcd','(.*)(?<=[bc])')
1
select regexp_like('abcd','(.*)(?<=[bc])c') /* Result: y */;
regexp_like('abcd','(.*)(?<=[bc])c')
1
select regexp_like('abcd','(.*?)c') /* Result: y */;
regexp_like('abcd','(.*?)c')
1
select regexp_like('abcd','(.*?)(?=c)') /* Result: y */;
regexp_like('abcd','(.*?)(?=c)')
1
select regexp_like('abcd','(.*?)(?=c)c') /* Result: yB */;
regexp_like('abcd','(.*?)(?=c)c')
1
select regexp_like('abcd','(.*?)(?=b|c)') /* Result: y */;
regexp_like('abcd','(.*?)(?=b|c)')
1
select regexp_like('abcd','(.*?)(?=b|c)c') /* Result: y */;
regexp_like('abcd','(.*?)(?=b|c)c')
1
select regexp_like('abcd','(.*?)(?=c|b)') /* Result: y */;
regexp_like('abcd','(.*?)(?=c|b)')
1
select regexp_like('abcd','(.*?)(?=c|b)c') /* Result: y */;
regexp_like('abcd','(.*?)(?=c|b)c')
1
select regexp_like('abcd','(.*?)(?=[bc])') /* Result: y */;
regexp_like('abcd','(.*?)(?=[bc])')
1
select regexp_like('abcd','(.*?)(?=[bc])c') /* Result: yB */;
regexp_like('abcd','(.*?)(?=[bc])c')
1
select regexp_like('abcd','(.*?)(?<=b)') /* Result: y */;
regexp_like('abcd','(.*?)(?<=b)')
1
select regexp_like('abcd','(.*?)(?<=b)c') /* Result: y */;
regexp_like('abcd','(.*?)(?<=b)c')
1
select regexp_like('abcd','(.*?)(?<=b|c)') /* Result: y */;
regexp_like('abcd','(.*?)(?<=b|c)')
1
select regexp_like('abcd','(.*?)(?<=b|c)c') /* Result: y */;
regexp_like('abcd','(.*?)(?<=b|c)c')
1
select regexp_like('abcd','(.*?)(?<=c|b)') /* Result: y */;
regexp_like('abcd','(.*?)(?<=c|b)')
1
select regexp_like('abcd','(.*?)(?<=c|b)c') /* Result: y */;
regexp_like('abcd','(.*?)(?<=c|b)c')
1
select regexp_like('abcd','(.*?)(?<=[bc])') /* Result: y */;
regexp_like('abcd','(.*?)(?<=[bc])')
1
select regexp_like('abcd','(.*?)(?<=[bc])c') /* Result: y */;
regexp_like('abcd','(.*?)(?<=[bc])c')
1
select regexp_like('2','2(]*)?$\\1') /* Result: y */;
regexp_like('2','2(]*)?$\\1')
1
select regexp_like('x','(??{})') /* Result: yi */;
ERROR HY000: Syntax error in regular expression on line 1, character 3.
