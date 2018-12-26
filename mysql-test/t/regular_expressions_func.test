## Test based on extra/icu/tests/testdata/re_test.txt
select regexp_like('abc','abc') /* Result: y */;
select regexp_like('xbc','abc') /* Result: n */;
select regexp_like('axc','abc') /* Result: n */;
select regexp_like('abx','abc') /* Result: n */;
select regexp_like('xabcy','abc') /* Result: y */;
select regexp_like('ababc','abc') /* Result: y */;
select regexp_like('abc','ab*c') /* Result: y */;
select regexp_like('abc','ab*bc') /* Result: y */;
select regexp_like('abbc','ab*bc') /* Result: y */;
select regexp_like('abbbbc','ab*bc') /* Result: y */;
select regexp_like('abbbbc','.{1}') /* Result: y */;
select regexp_like('abbbbc','.{3,4}') /* Result: y */;
select regexp_like('abbbbc','ab{0,}bc') /* Result: y */;
select regexp_like('abbc','ab+bc') /* Result: y */;
select regexp_like('abc','ab+bc') /* Result: n */;
select regexp_like('abq','ab+bc') /* Result: n */;
select regexp_like('abq','ab{1,}bc') /* Result: n */;
select regexp_like('abbbbc','ab+bc') /* Result: y */;
select regexp_like('abbbbc','ab{1,}bc') /* Result: y */;
select regexp_like('abbbbc','ab{1,3}bc') /* Result: y */;
select regexp_like('abbbbc','ab{3,4}bc') /* Result: y */;
select regexp_like('abbbbc','ab{4,5}bc') /* Result: n */;
select regexp_like('abbc','ab?bc') /* Result: y */;
select regexp_like('abc','ab?bc') /* Result: y */;
select regexp_like('abc','ab{0,1}bc') /* Result: y */;
select regexp_like('abbbbc','ab?bc') /* Result: n */;
select regexp_like('abc','ab?c') /* Result: y */;
select regexp_like('abc','ab{0,1}c') /* Result: y */;
select regexp_like('abc','^abc$') /* Result: y */;
select regexp_like('abcc','^abc$') /* Result: n */;
select regexp_like('abcc','^abc') /* Result: y */;
select regexp_like('aabc','^abc$') /* Result: n */;
select regexp_like('aabc','abc$') /* Result: y */;
select regexp_like('aabcd','abc$') /* Result: n */;
select regexp_like('abc','^') /* Result: y */;
select regexp_like('abc','$') /* Result: y */;
select regexp_like('abc','a.c') /* Result: y */;
select regexp_like('axc','a.c') /* Result: y */;
select regexp_like('axyzc','a.*c') /* Result: y */;
select regexp_like('axyzd','a.*c') /* Result: n */;
select regexp_like('abc','a[bc]d') /* Result: n */;
select regexp_like('abd','a[bc]d') /* Result: y */;
select regexp_like('abd','a[b-d]e') /* Result: n */;
select regexp_like('ace','a[b-d]e') /* Result: y */;
select regexp_like('aac','a[b-d]') /* Result: y */;
select regexp_like('a-','a[-b]') /* Result: y */;
select regexp_like('a-','a[b-]') /* Result: y */;
--error ER_REGEXP_INVALID_RANGE
select regexp_like('-','a[b-a]') /* Result: c */;
--error ER_REGEXP_MISSING_CLOSE_BRACKET
select regexp_like('-','a[]b') /* Result: ci */;
--error ER_REGEXP_MISSING_CLOSE_BRACKET
select regexp_like('-','a[') /* Result: c */;
select regexp_like('a]','a]') /* Result: y */;
select regexp_like('a]b','a[]]b') /* Result: y */;
select regexp_like('aed','a[^bc]d') /* Result: y */;
select regexp_like('abd','a[^bc]d') /* Result: n */;
select regexp_like('adc','a[^-b]c') /* Result: y */;
select regexp_like('a-c','a[^-b]c') /* Result: n */;
select regexp_like('a]c','a[^]b]c') /* Result: n */;
select regexp_like('adc','a[^]b]c') /* Result: y */;
select regexp_like('a-','\\ba\\b') /* Result: y */;
select regexp_like('-a','\\ba\\b') /* Result: y */;
select regexp_like('-a-','\\ba\\b') /* Result: y */;
select regexp_like('xy','\\by\\b') /* Result: n */;
select regexp_like('yz','\\by\\b') /* Result: n */;
select regexp_like('xyz','\\by\\b') /* Result: n */;
select regexp_like('a-','\\Ba\\B') /* Result: n */;
select regexp_like('-a','\\Ba\\B') /* Result: n */;
select regexp_like('-a-','\\Ba\\B') /* Result: n */;
select regexp_like('xy','\\By\\b') /* Result: y */;
select regexp_like('yz','\\by\\B') /* Result: y */;
select regexp_like('xyz','\\By\\B') /* Result: y */;
select regexp_like('a','\\w') /* Result: y */;
select regexp_like('-','\\w') /* Result: n */;
select regexp_like('a','\\W') /* Result: n */;
select regexp_like('-','\\W') /* Result: y */;
select regexp_like('a b','a\\sb') /* Result: y */;
select regexp_like('a-b','a\\sb') /* Result: n */;
select regexp_like('a b','a\\Sb') /* Result: n */;
select regexp_like('a-b','a\\Sb') /* Result: y */;
select regexp_like('1','\\d') /* Result: y */;
select regexp_like('-','\\d') /* Result: n */;
select regexp_like('1','\\D') /* Result: n */;
select regexp_like('-','\\D') /* Result: y */;
select regexp_like('a','[\\w]') /* Result: y */;
select regexp_like('-','[\\w]') /* Result: n */;
select regexp_like('a','[\\W]') /* Result: n */;
select regexp_like('-','[\\W]') /* Result: y */;
select regexp_like('a b','a[\\s]b') /* Result: y */;
select regexp_like('a-b','a[\\s]b') /* Result: n */;
select regexp_like('a b','a[\\S]b') /* Result: n */;
select regexp_like('a-b','a[\\S]b') /* Result: y */;
select regexp_like('1','[\\d]') /* Result: y */;
select regexp_like('-','[\\d]') /* Result: n */;
select regexp_like('1','[\\D]') /* Result: n */;
select regexp_like('-','[\\D]') /* Result: y */;
select regexp_like('abc','ab|cd') /* Result: y */;
select regexp_like('abcd','ab|cd') /* Result: y */;
select regexp_like('def','()ef') /* Result: y */;
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('-','*a') /* Result: c */;
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('-','(*)b') /* Result: c */;
select regexp_like('b','$b') /* Result: n */;
--error ER_REGEXP_BAD_ESCAPE_SEQUENCE
select regexp_like('-','a\\') /* Result: c */;
select regexp_like('a(b','a\\(b') /* Result: y */;
select regexp_like('ab','a\\(*b') /* Result: y */;
select regexp_like('a((b','a\\(*b') /* Result: y */;
select regexp_like('a\\b','a\\\\b') /* Result: y */;
--error ER_REGEXP_MISMATCHED_PAREN
select regexp_like('-','abc)') /* Result: c */;
--error ER_REGEXP_MISMATCHED_PAREN
select regexp_like('-','(abc') /* Result: c */;
select regexp_like('abc','((a))') /* Result: y */;
select regexp_like('abc','(a)b(c)') /* Result: y */;
select regexp_like('aabbabc','a+b+c') /* Result: y */;
select regexp_like('aabbabc','a{1,}b{1,}c') /* Result: y */;
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('-','a**') /* Result: c */;
select regexp_like('abcabc','a.+?c') /* Result: y */;
select regexp_like('ab','(a+|b)*') /* Result: y */;
select regexp_like('ab','(a+|b){0,}') /* Result: y */;
select regexp_like('ab','(a+|b)+') /* Result: y */;
select regexp_like('ab','(a+|b){1,}') /* Result: y */;
select regexp_like('ab','(a+|b)?') /* Result: y */;
select regexp_like('ab','(a+|b){0,1}') /* Result: y */;
--error ER_REGEXP_MISMATCHED_PAREN
select regexp_like('-',')(') /* Result: c */;
select regexp_like('cde','[^ab]*') /* Result: y */;
select regexp_like('','abc') /* Result: n */;
select regexp_like('','a*') /* Result: y */;
select regexp_like('abbbcd','([abc])*d') /* Result: y */;
select regexp_like('abcd','([abc])*bcd') /* Result: y */;
select regexp_like('e','a|b|c|d|e') /* Result: y */;
select regexp_like('ef','(a|b|c|d|e)f') /* Result: y */;
select regexp_like('abcdefg','abcd*efg') /* Result: y */;
select regexp_like('xabyabbbz','ab*') /* Result: y */;
select regexp_like('xayabbbz','ab*') /* Result: y */;
select regexp_like('abcde','(ab|cd)e') /* Result: y */;
select regexp_like('hij','[abhgefdc]ij') /* Result: y */;
select regexp_like('abcde','^(ab|cd)e') /* Result: n */;
select regexp_like('abcdef','(abc|)ef') /* Result: y */;
select regexp_like('abcd','(a|b)c*d') /* Result: y */;
select regexp_like('abc','(ab|ab*)bc') /* Result: y */;
select regexp_like('abc','a([bc]*)c*') /* Result: y */;
select regexp_like('abcd','a([bc]*)(c*d)') /* Result: y */;
select regexp_like('abcd','a([bc]+)(c*d)') /* Result: y */;
select regexp_like('abcd','a([bc]*)(c+d)') /* Result: y */;
select regexp_like('adcdcde','a[bcd]*dcdcde') /* Result: y */;
select regexp_like('adcdcde','a[bcd]+dcdcde') /* Result: n */;
select regexp_like('abc','(ab|a)b*c') /* Result: y */;
select regexp_like('abcd','((a)(b)c)(d)') /* Result: y */;
select regexp_like('alpha','[a-zA-Z_][a-zA-Z0-9_]*') /* Result: y */;
select regexp_like('abh','^a(bc+|b[eh])g|.h$') /* Result: y */;
select regexp_like('effgz','(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
select regexp_like('ij','(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
select regexp_like('effg','(bc+d$|ef*g.|h?i(j|k))') /* Result: n */;
select regexp_like('bcdd','(bc+d$|ef*g.|h?i(j|k))') /* Result: n */;
select regexp_like('reffgz','(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
select regexp_like('a','((((((((((a))))))))))') /* Result: y */;
select regexp_like('aa','((((((((((a))))))))))\\10') /* Result: y */;
select regexp_like('a','(((((((((a)))))))))') /* Result: y */;
select regexp_like('uh-uh','multiple words of text') /* Result: n */;
select regexp_like('multiple words, yeah','multiple words') /* Result: y */;
select regexp_like('abcde','(.*)c(.*)') /* Result: y */;
select regexp_like('(a, b)','\\((.*), (.*)\\)') /* Result: y */;
select regexp_like('ab','[k]') /* Result: n */;
select regexp_like('abcd','abcd') /* Result: y */;
select regexp_like('abcd','a(bc)d') /* Result: y */;
select regexp_like('ac','a[-]?c') /* Result: y */;
select regexp_like('abcabc','(abc)\\1') /* Result: y */;
select regexp_like('abcabc','([a-c]*)\\1') /* Result: y */;
--error ER_REGEXP_INVALID_BACK_REF
select regexp_like('-','\\1') /* Result: c */;
--error ER_REGEXP_INVALID_BACK_REF
select regexp_like('-','\\2') /* Result: c */;
select regexp_like('a','(a)|\\1') /* Result: y */;
select regexp_like('x','(a)|\\1') /* Result: n */;
--error ER_REGEXP_INVALID_BACK_REF
select regexp_like('-','(a)|\\2') /* Result: c */;
select regexp_like('ababbbcbc','(([a-c])b*?\\2)*') /* Result: y */;
select regexp_like('ababbbcbc','(([a-c])b*?\\2){3}') /* Result: y */;
select regexp_like('aaxabxbaxbbx','((\\3|b)\\2(a)x)+') /* Result: n */;
select regexp_like('aaaxabaxbaaxbbax','((\\3|b)\\2(a)x)+') /* Result: y */;
select regexp_like('bbaababbabaaaaabbaaaabba','((\\3|b)\\2(a)){2,}') /* Result: y */;
select regexp_like('b','(a)|(b)') /* Result: y */;
select regexp_like('ABC','(?i)abc') /* Result: y */;
select regexp_like('XBC','(?i)abc') /* Result: n */;
select regexp_like('AXC','(?i)abc') /* Result: n */;
select regexp_like('ABX','(?i)abc') /* Result: n */;
select regexp_like('XABCY','(?i)abc') /* Result: y */;
select regexp_like('ABABC','(?i)abc') /* Result: y */;
select regexp_like('ABC','(?i)ab*c') /* Result: y */;
select regexp_like('ABC','(?i)ab*bc') /* Result: y */;
select regexp_like('ABBC','(?i)ab*bc') /* Result: y */;
select regexp_like('ABBBBC','(?i)ab*?bc') /* Result: y */;
select regexp_like('ABBBBC','(?i)ab{0,}?bc') /* Result: y */;
select regexp_like('ABBC','(?i)ab+?bc') /* Result: y */;
select regexp_like('ABC','(?i)ab+bc') /* Result: n */;
select regexp_like('ABQ','(?i)ab+bc') /* Result: n */;
select regexp_like('ABQ','(?i)ab{1,}bc') /* Result: n */;
select regexp_like('ABBBBC','(?i)ab+bc') /* Result: y */;
select regexp_like('ABBBBC','(?i)ab{1,}?bc') /* Result: y */;
select regexp_like('ABBBBC','(?i)ab{1,3}?bc') /* Result: y */;
select regexp_like('ABBBBC','(?i)ab{3,4}?bc') /* Result: y */;
select regexp_like('ABBBBC','(?i)ab{4,5}?bc') /* Result: n */;
select regexp_like('ABBC','(?i)ab??bc') /* Result: y */;
select regexp_like('ABC','(?i)ab??bc') /* Result: y */;
select regexp_like('ABC','(?i)ab{0,1}?bc') /* Result: y */;
select regexp_like('ABBBBC','(?i)ab??bc') /* Result: n */;
select regexp_like('ABC','(?i)ab??c') /* Result: y */;
select regexp_like('ABC','(?i)ab{0,1}?c') /* Result: y */;
select regexp_like('ABC','(?i)^abc$') /* Result: y */;
select regexp_like('ABCC','(?i)^abc$') /* Result: n */;
select regexp_like('ABCC','(?i)^abc') /* Result: y */;
select regexp_like('AABC','(?i)^abc$') /* Result: n */;
select regexp_like('AABC','(?i)abc$') /* Result: y */;
select regexp_like('ABC','(?i)^') /* Result: y */;
select regexp_like('ABC','(?i)$') /* Result: y */;
select regexp_like('ABC','(?i)a.c') /* Result: y */;
select regexp_like('AXC','(?i)a.c') /* Result: y */;
select regexp_like('AXYZC','(?i)a.*?c') /* Result: y */;
select regexp_like('AXYZD','(?i)a.*c') /* Result: n */;
select regexp_like('ABC','(?i)a[bc]d') /* Result: n */;
select regexp_like('ABD','(?i)a[bc]d') /* Result: y */;
select regexp_like('ABD','(?i)a[b-d]e') /* Result: n */;
select regexp_like('ACE','(?i)a[b-d]e') /* Result: y */;
select regexp_like('AAC','(?i)a[b-d]') /* Result: y */;
select regexp_like('A-','(?i)a[-b]') /* Result: y */;
select regexp_like('A-','(?i)a[b-]') /* Result: y */;
--error ER_REGEXP_INVALID_RANGE
select regexp_like('-','(?i)a[b-a]') /* Result: c */;
--error ER_REGEXP_MISSING_CLOSE_BRACKET
select regexp_like('-','(?i)a[]b') /* Result: ci */;
--error ER_REGEXP_MISSING_CLOSE_BRACKET
select regexp_like('-','(?i)a[') /* Result: c */;
select regexp_like('A]','(?i)a]') /* Result: y */;
select regexp_like('A]B','(?i)a[]]b') /* Result: y */;
select regexp_like('AED','(?i)a[^bc]d') /* Result: y */;
select regexp_like('ABD','(?i)a[^bc]d') /* Result: n */;
select regexp_like('ADC','(?i)a[^-b]c') /* Result: y */;
select regexp_like('A-C','(?i)a[^-b]c') /* Result: n */;
select regexp_like('A]C','(?i)a[^]b]c') /* Result: n */;
select regexp_like('ADC','(?i)a[^]b]c') /* Result: y */;
select regexp_like('ABC','(?i)ab|cd') /* Result: y */;
select regexp_like('ABCD','(?i)ab|cd') /* Result: y */;
select regexp_like('DEF','(?i)()ef') /* Result: y */;
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('-','(?i)*a') /* Result: c */;
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('-','(?i)(*)b') /* Result: c */;
select regexp_like('B','(?i)$b') /* Result: n */;
--error ER_REGEXP_BAD_ESCAPE_SEQUENCE
select regexp_like('-','(?i)a\\') /* Result: c */;
select regexp_like('A(B','(?i)a\\(b') /* Result: y */;
select regexp_like('AB','(?i)a\\(*b') /* Result: y */;
select regexp_like('A((B','(?i)a\\(*b') /* Result: y */;
select regexp_like('A\\B','(?i)a\\\\b') /* Result: y */;
--error ER_REGEXP_MISMATCHED_PAREN
select regexp_like('-','(?i)abc)') /* Result: c */;
--error ER_REGEXP_MISMATCHED_PAREN
select regexp_like('-','(?i)(abc') /* Result: c */;
select regexp_like('ABC','(?i)((a))') /* Result: y */;
select regexp_like('ABC','(?i)(a)b(c)') /* Result: y */;
select regexp_like('AABBABC','(?i)a+b+c') /* Result: y */;
select regexp_like('AABBABC','(?i)a{1,}b{1,}c') /* Result: y */;
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('-','(?i)a**') /* Result: c */;
select regexp_like('ABCABC','(?i)a.+?c') /* Result: y */;
select regexp_like('ABCABC','(?i)a.*?c') /* Result: y */;
select regexp_like('ABCABC','(?i)a.{0,5}?c') /* Result: y */;
select regexp_like('AB','(?i)(a+|b)*') /* Result: y */;
select regexp_like('AB','(?i)(a+|b){0,}') /* Result: y */;
select regexp_like('AB','(?i)(a+|b)+') /* Result: y */;
select regexp_like('AB','(?i)(a+|b){1,}') /* Result: y */;
select regexp_like('AB','(?i)(a+|b)?') /* Result: y */;
select regexp_like('AB','(?i)(a+|b){0,1}') /* Result: y */;
select regexp_like('AB','(?i)(a+|b){0,1}?') /* Result: y */;
--error ER_REGEXP_MISMATCHED_PAREN
select regexp_like('-','(?i))(') /* Result: c */;
select regexp_like('CDE','(?i)[^ab]*') /* Result: y */;
select regexp_like('','(?i)abc') /* Result: n */;
select regexp_like('','(?i)a*') /* Result: y */;
select regexp_like('ABBBCD','(?i)([abc])*d') /* Result: y */;
select regexp_like('ABCD','(?i)([abc])*bcd') /* Result: y */;
select regexp_like('E','(?i)a|b|c|d|e') /* Result: y */;
select regexp_like('EF','(?i)(a|b|c|d|e)f') /* Result: y */;
select regexp_like('ABCDEFG','(?i)abcd*efg') /* Result: y */;
select regexp_like('XABYABBBZ','(?i)ab*') /* Result: y */;
select regexp_like('XAYABBBZ','(?i)ab*') /* Result: y */;
select regexp_like('ABCDE','(?i)(ab|cd)e') /* Result: y */;
select regexp_like('HIJ','(?i)[abhgefdc]ij') /* Result: y */;
select regexp_like('ABCDE','(?i)^(ab|cd)e') /* Result: n */;
select regexp_like('ABCDEF','(?i)(abc|)ef') /* Result: y */;
select regexp_like('ABCD','(?i)(a|b)c*d') /* Result: y */;
select regexp_like('ABC','(?i)(ab|ab*)bc') /* Result: y */;
select regexp_like('ABC','(?i)a([bc]*)c*') /* Result: y */;
select regexp_like('ABCD','(?i)a([bc]*)(c*d)') /* Result: y */;
select regexp_like('ABCD','(?i)a([bc]+)(c*d)') /* Result: y */;
select regexp_like('ABCD','(?i)a([bc]*)(c+d)') /* Result: y */;
select regexp_like('ADCDCDE','(?i)a[bcd]*dcdcde') /* Result: y */;
select regexp_like('ADCDCDE','(?i)a[bcd]+dcdcde') /* Result: n */;
select regexp_like('ABC','(?i)(ab|a)b*c') /* Result: y */;
select regexp_like('ABCD','(?i)((a)(b)c)(d)') /* Result: y */;
select regexp_like('ALPHA','(?i)[a-zA-Z_][a-zA-Z0-9_]*') /* Result: y */;
select regexp_like('ABH','(?i)^a(bc+|b[eh])g|.h$') /* Result: y */;
select regexp_like('EFFGZ','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
select regexp_like('IJ','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
select regexp_like('EFFG','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: n */;
select regexp_like('BCDD','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: n */;
select regexp_like('REFFGZ','(?i)(bc+d$|ef*g.|h?i(j|k))') /* Result: y */;
select regexp_like('A','(?i)((((((((((a))))))))))') /* Result: y */;
select regexp_like('AA','(?i)((((((((((a))))))))))\\10') /* Result: y */;
select regexp_like('A','(?i)(((((((((a)))))))))') /* Result: y */;
select regexp_like('A','(?i)(?:(?:(?:(?:(?:(?:(?:(?:(?:(a))))))))))') /* Result: y */;
select regexp_like('C','(?i)(?:(?:(?:(?:(?:(?:(?:(?:(?:(a|b|c))))))))))') /* Result: y */;
select regexp_like('UH-UH','(?i)multiple words of text') /* Result: n */;
select regexp_like('MULTIPLE WORDS, YEAH','(?i)multiple words') /* Result: y */;
select regexp_like('ABCDE','(?i)(.*)c(.*)') /* Result: y */;
select regexp_like('(A, B)','(?i)\\((.*), (.*)\\)') /* Result: y */;
select regexp_like('AB','(?i)[k]') /* Result: n */;
select regexp_like('ABCD','(?i)abcd') /* Result: y */;
select regexp_like('ABCD','(?i)a(bc)d') /* Result: y */;
select regexp_like('AC','(?i)a[-]?c') /* Result: y */;
select regexp_like('ABCABC','(?i)(abc)\\1') /* Result: y */;
select regexp_like('ABCABC','(?i)([a-c]*)\\1') /* Result: y */;
select regexp_like('abad','a(?!b).') /* Result: y */;
select regexp_like('abad','a(?=d).') /* Result: y */;
select regexp_like('abad','a(?=c|d).') /* Result: y */;
select regexp_like('ace','a(?:b|c|d)(.)') /* Result: y */;
select regexp_like('ace','a(?:b|c|d)*(.)') /* Result: y */;
select regexp_like('ace','a(?:b|c|d)+?(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d)+?(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d)+(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d){2}(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d){4,5}(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d){4,5}?(.)') /* Result: y */;
select regexp_like('foobar','((foo)|(bar))*') /* Result: y */;
--error ER_REGEXP_MISMATCHED_PAREN
select regexp_like('-',':(?:') /* Result: c */;
select regexp_like('acdbcdbe','a(?:b|c|d){6,7}(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d){6,7}?(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d){5,6}(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d){5,6}?(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d){5,7}(.)') /* Result: y */;
select regexp_like('acdbcdbe','a(?:b|c|d){5,7}?(.)') /* Result: y */;
select regexp_like('ace','a(?:b|(c|e){1,2}?|d)+?(.)') /* Result: y */;
select regexp_like('AB','^(.+)?B') /* Result: y */;
select regexp_like('.','^([^a-z])|(\\^)$') /* Result: y */;
select regexp_like('<&OUT','^[<>]&') /* Result: y */;
select regexp_like('aaaaaaaaaa','^(a\\1?){4}$') /* Result: y */;
select regexp_like('aaaaaaaaa','^(a\\1?){4}$') /* Result: n */;
select regexp_like('aaaaaaaaaaa','^(a\\1?){4}$') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('aaaaaaaaaa','^(a(?(1)\\1)){4}$') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('aaaaaaaaa','^(a(?(1)\\1)){4}$') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('aaaaaaaaaaa','^(a(?(1)\\1)){4}$') /* Result: n */;
select regexp_like('aaaaaaaaa','((a{4})+)') /* Result: y */;
select regexp_like('aaaaaaaaaa','(((aa){2})+)') /* Result: y */;
select regexp_like('aaaaaaaaaa','(((a{2}){2})+)') /* Result: y */;
select regexp_like('foobar','(?:(f)(o)(o)|(b)(a)(r))*') /* Result: y */;
select regexp_like('ab','(?<=a)b') /* Result: y */;
select regexp_like('cb','(?<=a)b') /* Result: n */;
select regexp_like('b','(?<=a)b') /* Result: n */;
select regexp_like('ab','(?<!c)b') /* Result: y */;
select regexp_like('cb','(?<!c)b') /* Result: n */;
select regexp_like('b','(?<!c)b') /* Result: y */;
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('-','(?<%)b') /* Result: c */;
select regexp_like('aba','(?:..)*a') /* Result: y */;
select regexp_like('aba','(?:..)*?a') /* Result: y */;
select regexp_like('abc','^(?:b|a(?=(.)))*\\1') /* Result: y */;
select regexp_like('abc','^(){3,5}') /* Result: y */;
select regexp_like('aax','^(a+)*ax') /* Result: y */;
select regexp_like('aax','^((a|b)+)*ax') /* Result: y */;
select regexp_like('aax','^((a|bc)+)*ax') /* Result: y */;
select regexp_like('cab','(a|x)*ab') /* Result: y */;
select regexp_like('cab','(a)*ab') /* Result: y */;
select regexp_like('ab','(?:(?i)a)b') /* Result: y */;
select regexp_like('ab','((?i)a)b') /* Result: y */;
select regexp_like('Ab','(?:(?i)a)b') /* Result: y */;
select regexp_like('Ab','((?i)a)b') /* Result: y */;
select regexp_like('aB','(?:(?i)a)b') /* Result: n */;
select regexp_like('aB','((?i)a)b') /* Result: n */;
select regexp_like('ab','(?i:a)b') /* Result: y */;
select regexp_like('ab','((?i:a))b') /* Result: y */;
select regexp_like('Ab','(?i:a)b') /* Result: y */;
select regexp_like('Ab','((?i:a))b') /* Result: y */;
select regexp_like('aB','(?i:a)b') /* Result: n */;
select regexp_like('aB','((?i:a))b') /* Result: n */;
select regexp_like('ab','(?i)(?:(?-i)a)b') /* Result: y */;
select regexp_like('ab','(?i)((?-i)a)b') /* Result: y */;
select regexp_like('aB','(?i)(?:(?-i)a)b') /* Result: y */;
select regexp_like('aB','(?i)((?-i)a)b') /* Result: y */;
select regexp_like('Ab','(?i)(?:(?-i)a)b') /* Result: n */;
select regexp_like('Ab','(?i)((?-i)a)b') /* Result: n */;
select regexp_like('AB','(?i)(?:(?-i)a)b') /* Result: n */;
select regexp_like('AB','(?i)((?-i)a)b') /* Result: n */;
select regexp_like('ab','(?i)(?-i:a)b') /* Result: y */;
select regexp_like('ab','(?i)((?-i:a))b') /* Result: y */;
select regexp_like('aB','(?i)(?-i:a)b') /* Result: y */;
select regexp_like('aB','(?i)((?-i:a))b') /* Result: y */;
select regexp_like('Ab','(?i)(?-i:a)b') /* Result: n */;
select regexp_like('Ab','(?i)((?-i:a))b') /* Result: n */;
select regexp_like('AB','(?i)(?-i:a)b') /* Result: n */;
select regexp_like('AB','(?i)((?-i:a))b') /* Result: n */;
select regexp_like('a\nB','(?i)((?-i:a.))b') /* Result: n */;
select regexp_like('a\nB','(?i)((?s-i:a.))b') /* Result: y */;
select regexp_like('B\nB','(?i)((?s-i:a.))b') /* Result: n */;
select regexp_like('cabbbb','(?:c|d)(?:)(?:a(?:)(?:b)(?:b(?:))(?:b(?:)(?:b)))') /* Result: y */;
select regexp_like('caaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','(?:c|d)(?:)(?:aaaaaaaa(?:)(?:bbbbbbbb)(?:bbbbbbbb(?:))(?:bbbbbbbb(?:)(?:bbbbbbbb)))') /* Result: y */;
select regexp_like('Ab4ab','(?i)(ab)\\d\\1') /* Result: y */;
select regexp_like('ab4Ab','(?i)(ab)\\d\\1') /* Result: y */;
select regexp_like('foobar1234baz','foo\\w*\\d{4}baz') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('cabd','a(?{})b') /* Result: y */;
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('-','a(?{)b') /* Result: c */;
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('-','a(?{{})b') /* Result: c */;
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('-','a(?{}})b') /* Result: c */;
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('-','a(?{"{"})b') /* Result: c */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('cabd','a(?{"\\{"})b') /* Result: y */;
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('-','a(?{"{"}})b') /* Result: c */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('caxbd','a(?{$bl="\\{"}).b') /* Result: y */;
select regexp_like('x~~','x(~~)*(?:(?:F)?)?') /* Result: y */;
select regexp_like('aaac','^a(?#xxx){3}c') /* Result: y */;
select regexp_like('aaac','(?x)^a (?#xxx) (?#yyy) {3}c') /* Result: y */;
select regexp_like('dbcb','(?<![cd])b') /* Result: n */;
select regexp_like('dbaacb','(?<![cd])[ab]') /* Result: y */;
select regexp_like('dbcb','(?<!(c|d))b') /* Result: n */;
select regexp_like('dbaacb','(?<!(c|d))[ab]') /* Result: y */;
select regexp_like('cdaccb','(?<!cd)[ab]') /* Result: y */;
select regexp_like('a--','^(?:a?b?)*$') /* Result: n */;
select regexp_like('a\nb\nc\n','((?s)^a(.))((?m)^b$)') /* Result: y */;
select regexp_like('a\nb\nc\n','((?m)^b$)') /* Result: y */;
select regexp_like('a\nb\n','(?m)^b') /* Result: y */;
select regexp_like('a\nb\n','(?m)^(b)') /* Result: y */;
select regexp_like('a\nb\n','((?m)^b)') /* Result: y */;
select regexp_like('a\nb\n','\n((?m)^b)') /* Result: y */;
select regexp_like('a\nb\nc\n','((?s).)c(?!.)') /* Result: y */;
select regexp_like('a\nb\nc\n','((?s)b.)c(?!.)') /* Result: y */;
select regexp_like('a\nb\nc\n','^b') /* Result: n */;
select regexp_like('a\nb\nc\n','()^b') /* Result: n */;
select regexp_like('a\nb\nc\n','((?m)^b)') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(1)a|b)') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(1)b|a)') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(x)?(?(1)a|b)') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(x)?(?(1)b|a)') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','()?(?(1)b|a)') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','()(?(1)b|a)') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','()?(?(1)a|b)') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('(blah)','^(\\()?blah(?(1)(\\)))$') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('blah','^(\\()?blah(?(1)(\\)))$') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('blah)','^(\\()?blah(?(1)(\\)))$') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('(blah','^(\\()?blah(?(1)(\\)))$') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('(blah)','^(\\(+)?blah(?(1)(\\)))$') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('blah','^(\\(+)?blah(?(1)(\\)))$') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('blah)','^(\\(+)?blah(?(1)(\\)))$') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('(blah','^(\\(+)?blah(?(1)(\\)))$') /* Result: n */;
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(1?)a|b)') /* Result: c */;
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(1)a|b|c)') /* Result: c */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(?{0})a|b)') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(?{0})b|a)') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(?{1})b|a)') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(?{1})a|b)') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(?!a)a|b)') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(?!a)b|a)') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(?=a)b|a)') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','(?(?=a)a|b)') /* Result: y */;
select regexp_like('aaab','(?=(a+?))(\\1ab)') /* Result: y */;
select regexp_like('aaab','^(?=(a+?))\\1ab') /* Result: n */;
select regexp_like('one:','(\\w+:)+') /* Result: y */;
select regexp_like('a','$(?<=^(a))') /* Result: y */;
select regexp_like('abcd:','([\\w:]+::)?(\\w+)$') /* Result: n */;
select regexp_like('abcd','([\\w:]+::)?(\\w+)$') /* Result: y */;
select regexp_like('xy:z:::abcd','([\\w:]+::)?(\\w+)$') /* Result: y */;
select regexp_like('aexycd','^[^bcd]*(c+)') /* Result: y */;
select regexp_like('caab','(a*)b+') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('yaaxxaaaacd','(?{$a=2})a*aa(?{local$a=$a+1})k*c(?{$b=$a})') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('yaaxxaaaacd','(?{$a=2})(a(?{local$a=$a+1}))*aak*c(?{$b=$a})') /* Result: y */;
select regexp_like('aaab','(>a+)ab') /* Result: n */;
select regexp_like('aaab','(?>a+)b') /* Result: y */;
select regexp_like('a:[b]:','([\\[:]+)') /* Result: yi */;
select regexp_like('a=[b]=','([\\[=]+)') /* Result: yi */;
select regexp_like('a.[b].','([\\[.]+)') /* Result: yi */;
--error ER_REGEXP_MISSING_CLOSE_BRACKET
select regexp_like('-','[a[:xyz:') /* Result: c */;
--error ER_REGEXP_ILLEGAL_ARGUMENT
select regexp_like('-','[a[:xyz:]') /* Result: c */;
select regexp_like('abc','[a\\[:]b[:c]') /* Result: yi */;
--error ER_REGEXP_ILLEGAL_ARGUMENT
select regexp_like('pbaq','([a[:xyz:]b]+)') /* Result: c */;
select regexp_like('abc','[a\\[:]b[:c]') /* Result: iy */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:alpha:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:alnum:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:ascii:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:cntrl:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:digit:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:graph:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:lower:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:print:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:punct:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:space:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:word:]]+)') /* Result: yi */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:upper:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:xdigit:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^alpha:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^alnum:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^ascii:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^cntrl:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^digit:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^lower:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^print:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^punct:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^space:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^word:]]+)') /* Result: yi */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^upper:]]+)') /* Result: y */;
select regexp_like(concat('ABcd01Xy__--  ', _utf16 x'0000ffff'),'([[:^xdigit:]]+)') /* Result: y */;
--error ER_REGEXP_ILLEGAL_ARGUMENT
select regexp_like('-','[[:foo:]]') /* Result: c */;
--error ER_REGEXP_ILLEGAL_ARGUMENT
select regexp_like('-','[[:^foo:]]') /* Result: c */;
select regexp_like('aaab','((?>a+)b)') /* Result: y */;
select regexp_like('aaab','(?>(a+))b') /* Result: y */;
select regexp_like('((abc(ade)ufh()()x','((?>[^()]+)|\\([^()]*\\))+') /* Result: y */;
--error ER_REGEXP_LOOK_BEHIND_LIMIT
select regexp_like('-','(?<=x+)y') /* Result: c */;
--error ER_REGEXP_MAX_LT_MIN
select regexp_like('-','a{37,17}') /* Result: c */;
select regexp_like('a\nb\n','\\Z') /* Result: y */;
select regexp_like('a\nb\n','\\z') /* Result: y */;
select regexp_like('a\nb\n','$') /* Result: y */;
select regexp_like('b\na\n','\\Z') /* Result: y */;
select regexp_like('b\na\n','\\z') /* Result: y */;
select regexp_like('b\na\n','$') /* Result: y */;
select regexp_like('b\na','\\Z') /* Result: y */;
select regexp_like('b\na','\\z') /* Result: y */;
select regexp_like('b\na','$') /* Result: y */;
select regexp_like('a\nb\n','(?m)\\Z') /* Result: y */;
select regexp_like('a\nb\n','(?m)\\z') /* Result: y */;
select regexp_like('a\nb\n','(?m)$') /* Result: y */;
select regexp_like('b\na\n','(?m)\\Z') /* Result: y */;
select regexp_like('b\na\n','(?m)\\z') /* Result: y */;
select regexp_like('b\na\n','(?m)$') /* Result: y */;
select regexp_like('b\na','(?m)\\Z') /* Result: y */;
select regexp_like('b\na','(?m)\\z') /* Result: y */;
select regexp_like('b\na','(?m)$') /* Result: y */;
select regexp_like('a\nb\n','a\\Z') /* Result: n */;
select regexp_like('a\nb\n','a\\z') /* Result: n */;
select regexp_like('a\nb\n','a$') /* Result: n */;
select regexp_like('b\na\n','a\\Z') /* Result: y */;
select regexp_like('b\na\n','a\\z') /* Result: n */;
select regexp_like('b\na\n','a$') /* Result: y */;
select regexp_like('b\na','a\\Z') /* Result: y */;
select regexp_like('b\na','a\\z') /* Result: y */;
select regexp_like('b\na','a$') /* Result: y */;
select regexp_like('a\nb\n','(?m)a\\Z') /* Result: n */;
select regexp_like('a\nb\n','(?m)a\\z') /* Result: n */;
select regexp_like('a\nb\n','(?m)a$') /* Result: y */;
select regexp_like('b\na\n','(?m)a\\Z') /* Result: y */;
select regexp_like('b\na\n','(?m)a\\z') /* Result: n */;
select regexp_like('b\na\n','(?m)a$') /* Result: y */;
select regexp_like('b\na','(?m)a\\Z') /* Result: y */;
select regexp_like('b\na','(?m)a\\z') /* Result: y */;
select regexp_like('b\na','(?m)a$') /* Result: y */;
select regexp_like('aa\nb\n','aa\\Z') /* Result: n */;
select regexp_like('aa\nb\n','aa\\z') /* Result: n */;
select regexp_like('aa\nb\n','aa$') /* Result: n */;
select regexp_like('b\naa\n','aa\\Z') /* Result: y */;
select regexp_like('b\naa\n','aa\\z') /* Result: n */;
select regexp_like('b\naa\n','aa$') /* Result: y */;
select regexp_like('b\naa','aa\\Z') /* Result: y */;
select regexp_like('b\naa','aa\\z') /* Result: y */;
select regexp_like('b\naa','aa$') /* Result: y */;
select regexp_like('aa\nb\n','(?m)aa\\Z') /* Result: n */;
select regexp_like('aa\nb\n','(?m)aa\\z') /* Result: n */;
select regexp_like('aa\nb\n','(?m)aa$') /* Result: y */;
select regexp_like('b\naa\n','(?m)aa\\Z') /* Result: y */;
select regexp_like('b\naa\n','(?m)aa\\z') /* Result: n */;
select regexp_like('b\naa\n','(?m)aa$') /* Result: y */;
select regexp_like('b\naa','(?m)aa\\Z') /* Result: y */;
select regexp_like('b\naa','(?m)aa\\z') /* Result: y */;
select regexp_like('b\naa','(?m)aa$') /* Result: y */;
select regexp_like('ac\nb\n','aa\\Z') /* Result: n */;
select regexp_like('ac\nb\n','aa\\z') /* Result: n */;
select regexp_like('ac\nb\n','aa$') /* Result: n */;
select regexp_like('b\nac\n','aa\\Z') /* Result: n */;
select regexp_like('b\nac\n','aa\\z') /* Result: n */;
select regexp_like('b\nac\n','aa$') /* Result: n */;
select regexp_like('b\nac','aa\\Z') /* Result: n */;
select regexp_like('b\nac','aa\\z') /* Result: n */;
select regexp_like('b\nac','aa$') /* Result: n */;
select regexp_like('ac\nb\n','(?m)aa\\Z') /* Result: n */;
select regexp_like('ac\nb\n','(?m)aa\\z') /* Result: n */;
select regexp_like('ac\nb\n','(?m)aa$') /* Result: n */;
select regexp_like('b\nac\n','(?m)aa\\Z') /* Result: n */;
select regexp_like('b\nac\n','(?m)aa\\z') /* Result: n */;
select regexp_like('b\nac\n','(?m)aa$') /* Result: n */;
select regexp_like('b\nac','(?m)aa\\Z') /* Result: n */;
select regexp_like('b\nac','(?m)aa\\z') /* Result: n */;
select regexp_like('b\nac','(?m)aa$') /* Result: n */;
select regexp_like('ca\nb\n','aa\\Z') /* Result: n */;
select regexp_like('ca\nb\n','aa\\z') /* Result: n */;
select regexp_like('ca\nb\n','aa$') /* Result: n */;
select regexp_like('b\nca\n','aa\\Z') /* Result: n */;
select regexp_like('b\nca\n','aa\\z') /* Result: n */;
select regexp_like('b\nca\n','aa$') /* Result: n */;
select regexp_like('b\nca','aa\\Z') /* Result: n */;
select regexp_like('b\nca','aa\\z') /* Result: n */;
select regexp_like('b\nca','aa$') /* Result: n */;
select regexp_like('ca\nb\n','(?m)aa\\Z') /* Result: n */;
select regexp_like('ca\nb\n','(?m)aa\\z') /* Result: n */;
select regexp_like('ca\nb\n','(?m)aa$') /* Result: n */;
select regexp_like('b\nca\n','(?m)aa\\Z') /* Result: n */;
select regexp_like('b\nca\n','(?m)aa\\z') /* Result: n */;
select regexp_like('b\nca\n','(?m)aa$') /* Result: n */;
select regexp_like('b\nca','(?m)aa\\Z') /* Result: n */;
select regexp_like('b\nca','(?m)aa\\z') /* Result: n */;
select regexp_like('b\nca','(?m)aa$') /* Result: n */;
select regexp_like('ab\nb\n','ab\\Z') /* Result: n */;
select regexp_like('ab\nb\n','ab\\z') /* Result: n */;
select regexp_like('ab\nb\n','ab$') /* Result: n */;
select regexp_like('b\nab\n','ab\\Z') /* Result: y */;
select regexp_like('b\nab\n','ab\\z') /* Result: n */;
select regexp_like('b\nab\n','ab$') /* Result: y */;
select regexp_like('b\nab','ab\\Z') /* Result: y */;
select regexp_like('b\nab','ab\\z') /* Result: y */;
select regexp_like('b\nab','ab$') /* Result: y */;
select regexp_like('ab\nb\n','(?m)ab\\Z') /* Result: n */;
select regexp_like('ab\nb\n','(?m)ab\\z') /* Result: n */;
select regexp_like('ab\nb\n','(?m)ab$') /* Result: y */;
select regexp_like('b\nab\n','(?m)ab\\Z') /* Result: y */;
select regexp_like('b\nab\n','(?m)ab\\z') /* Result: n */;
select regexp_like('b\nab\n','(?m)ab$') /* Result: y */;
select regexp_like('b\nab','(?m)ab\\Z') /* Result: y */;
select regexp_like('b\nab','(?m)ab\\z') /* Result: y */;
select regexp_like('b\nab','(?m)ab$') /* Result: y */;
select regexp_like('ac\nb\n','ab\\Z') /* Result: n */;
select regexp_like('ac\nb\n','ab\\z') /* Result: n */;
select regexp_like('ac\nb\n','ab$') /* Result: n */;
select regexp_like('b\nac\n','ab\\Z') /* Result: n */;
select regexp_like('b\nac\n','ab\\z') /* Result: n */;
select regexp_like('b\nac\n','ab$') /* Result: n */;
select regexp_like('b\nac','ab\\Z') /* Result: n */;
select regexp_like('b\nac','ab\\z') /* Result: n */;
select regexp_like('b\nac','ab$') /* Result: n */;
select regexp_like('ac\nb\n','(?m)ab\\Z') /* Result: n */;
select regexp_like('ac\nb\n','(?m)ab\\z') /* Result: n */;
select regexp_like('ac\nb\n','(?m)ab$') /* Result: n */;
select regexp_like('b\nac\n','(?m)ab\\Z') /* Result: n */;
select regexp_like('b\nac\n','(?m)ab\\z') /* Result: n */;
select regexp_like('b\nac\n','(?m)ab$') /* Result: n */;
select regexp_like('b\nac','(?m)ab\\Z') /* Result: n */;
select regexp_like('b\nac','(?m)ab\\z') /* Result: n */;
select regexp_like('b\nac','(?m)ab$') /* Result: n */;
select regexp_like('ca\nb\n','ab\\Z') /* Result: n */;
select regexp_like('ca\nb\n','ab\\z') /* Result: n */;
select regexp_like('ca\nb\n','ab$') /* Result: n */;
select regexp_like('b\nca\n','ab\\Z') /* Result: n */;
select regexp_like('b\nca\n','ab\\z') /* Result: n */;
select regexp_like('b\nca\n','ab$') /* Result: n */;
select regexp_like('b\nca','ab\\Z') /* Result: n */;
select regexp_like('b\nca','ab\\z') /* Result: n */;
select regexp_like('b\nca','ab$') /* Result: n */;
select regexp_like('ca\nb\n','(?m)ab\\Z') /* Result: n */;
select regexp_like('ca\nb\n','(?m)ab\\z') /* Result: n */;
select regexp_like('ca\nb\n','(?m)ab$') /* Result: n */;
select regexp_like('b\nca\n','(?m)ab\\Z') /* Result: n */;
select regexp_like('b\nca\n','(?m)ab\\z') /* Result: n */;
select regexp_like('b\nca\n','(?m)ab$') /* Result: n */;
select regexp_like('b\nca','(?m)ab\\Z') /* Result: n */;
select regexp_like('b\nca','(?m)ab\\z') /* Result: n */;
select regexp_like('b\nca','(?m)ab$') /* Result: n */;
select regexp_like('abb\nb\n','abb\\Z') /* Result: n */;
select regexp_like('abb\nb\n','abb\\z') /* Result: n */;
select regexp_like('abb\nb\n','abb$') /* Result: n */;
select regexp_like('b\nabb\n','abb\\Z') /* Result: y */;
select regexp_like('b\nabb\n','abb\\z') /* Result: n */;
select regexp_like('b\nabb\n','abb$') /* Result: y */;
select regexp_like('b\nabb','abb\\Z') /* Result: y */;
select regexp_like('b\nabb','abb\\z') /* Result: y */;
select regexp_like('b\nabb','abb$') /* Result: y */;
select regexp_like('abb\nb\n','(?m)abb\\Z') /* Result: n */;
select regexp_like('abb\nb\n','(?m)abb\\z') /* Result: n */;
select regexp_like('abb\nb\n','(?m)abb$') /* Result: y */;
select regexp_like('b\nabb\n','(?m)abb\\Z') /* Result: y */;
select regexp_like('b\nabb\n','(?m)abb\\z') /* Result: n */;
select regexp_like('b\nabb\n','(?m)abb$') /* Result: y */;
select regexp_like('b\nabb','(?m)abb\\Z') /* Result: y */;
select regexp_like('b\nabb','(?m)abb\\z') /* Result: y */;
select regexp_like('b\nabb','(?m)abb$') /* Result: y */;
select regexp_like('ac\nb\n','abb\\Z') /* Result: n */;
select regexp_like('ac\nb\n','abb\\z') /* Result: n */;
select regexp_like('ac\nb\n','abb$') /* Result: n */;
select regexp_like('b\nac\n','abb\\Z') /* Result: n */;
select regexp_like('b\nac\n','abb\\z') /* Result: n */;
select regexp_like('b\nac\n','abb$') /* Result: n */;
select regexp_like('b\nac','abb\\Z') /* Result: n */;
select regexp_like('b\nac','abb\\z') /* Result: n */;
select regexp_like('b\nac','abb$') /* Result: n */;
select regexp_like('ac\nb\n','(?m)abb\\Z') /* Result: n */;
select regexp_like('ac\nb\n','(?m)abb\\z') /* Result: n */;
select regexp_like('ac\nb\n','(?m)abb$') /* Result: n */;
select regexp_like('b\nac\n','(?m)abb\\Z') /* Result: n */;
select regexp_like('b\nac\n','(?m)abb\\z') /* Result: n */;
select regexp_like('b\nac\n','(?m)abb$') /* Result: n */;
select regexp_like('b\nac','(?m)abb\\Z') /* Result: n */;
select regexp_like('b\nac','(?m)abb\\z') /* Result: n */;
select regexp_like('b\nac','(?m)abb$') /* Result: n */;
select regexp_like('ca\nb\n','abb\\Z') /* Result: n */;
select regexp_like('ca\nb\n','abb\\z') /* Result: n */;
select regexp_like('ca\nb\n','abb$') /* Result: n */;
select regexp_like('b\nca\n','abb\\Z') /* Result: n */;
select regexp_like('b\nca\n','abb\\z') /* Result: n */;
select regexp_like('b\nca\n','abb$') /* Result: n */;
select regexp_like('b\nca','abb\\Z') /* Result: n */;
select regexp_like('b\nca','abb\\z') /* Result: n */;
select regexp_like('b\nca','abb$') /* Result: n */;
select regexp_like('ca\nb\n','(?m)abb\\Z') /* Result: n */;
select regexp_like('ca\nb\n','(?m)abb\\z') /* Result: n */;
select regexp_like('ca\nb\n','(?m)abb$') /* Result: n */;
select regexp_like('b\nca\n','(?m)abb\\Z') /* Result: n */;
select regexp_like('b\nca\n','(?m)abb\\z') /* Result: n */;
select regexp_like('b\nca\n','(?m)abb$') /* Result: n */;
select regexp_like('b\nca','(?m)abb\\Z') /* Result: n */;
select regexp_like('b\nca','(?m)abb\\z') /* Result: n */;
select regexp_like('b\nca','(?m)abb$') /* Result: n */;
select regexp_like('ca','(^|x)(c)') /* Result: y */;
select regexp_like('x','a*abc?xyz+pqr{3}ab{2,}xy{4,5}pq{0,6}AB{0,}zz') /* Result: n */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('yabz','a(?{$a=2;$b=3;($b)=$a})b') /* Result: y */;
select regexp_like('_I(round(xs * sz),1)','round\\(((?>[^()]+))\\)') /* Result: y */;
select regexp_like('x ','(?x)((?x:.) )') /* Result: y */;
select regexp_like('x ','(?x)((?-x:.) )') /* Result: y */;
select regexp_like('foo.bart','foo.bart') /* Result: y */;
select regexp_like('abcd\ndxxx','(?m)^d[x][x][x]') /* Result: y */;
select regexp_like('xxxtt','tt+$') /* Result: y */;
select regexp_like('za-9z','([a\\-\\d]+)') /* Result: yi */;
select regexp_like('a0-za','([\\d-z]+)') /* Result: y */;
select regexp_like('a0- z','([\\d-\\s]+)') /* Result: y */;
select regexp_like('za-9z','([a-[:digit:]]+)') /* Result: y */;
select regexp_like('=0-z=','([[:digit:]-z]+)') /* Result: y */;
select regexp_like('=0-z=','([[:digit:]-[:alpha:]]+)') /* Result: iy */;
select regexp_like('aaaXbX','\\GX.*X') /* Result: n */;
select regexp_like('3.1415926','(\\d+\\.\\d+)') /* Result: y */;
select regexp_like('have a web browser','(\\ba.{0,10}br)') /* Result: y */;
select regexp_like('Changes','(?i)\\.c(pp|xx|c)?$') /* Result: n */;
select regexp_like('IO.c','(?i)\\.c(pp|xx|c)?$') /* Result: y */;
select regexp_like('IO.c','(?i)(\\.c(pp|xx|c)?$)') /* Result: y */;
select regexp_like('C:/','^([a-z]:)') /* Result: n */;
select regexp_like('\nx aa','(?m)^\\S\\s+aa$') /* Result: y */;
select regexp_like('ab','(^|a)b') /* Result: y */;
select regexp_like('abac','^([ab]*?)(b)?(c)$') /* Result: y */;
select regexp_like('abcab','(\\w)?(abc)\\1b') /* Result: n */;
select regexp_like('a,b,c','^(?:.,){2}c') /* Result: y */;
select regexp_like('a,b,c','^(.,){2}c') /* Result: y */;
select regexp_like('a,b,c','^(?:[^,]*,){2}c') /* Result: y */;
select regexp_like('a,b,c','^([^,]*,){2}c') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]*,){3}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]*,){3,}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]*,){0,3}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{1,3},){3}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{1,3},){3,}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{1,3},){0,3}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{1,},){3}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{1,},){3,}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{1,},){0,3}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{0,3},){3}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{0,3},){3,}d') /* Result: y */;
select regexp_like('aaa,b,c,d','^([^,]{0,3},){0,3}d') /* Result: y */;
select regexp_like('','(?i)') /* Result: y */;
select regexp_like('a\nxb\n','(?m)(?!\\A)x') /* Result: y */;
select regexp_like('aba','^(a(b)?)+$') /* Result: yi */;
select regexp_like('123\nabcabcabcabc\n','(?m)^.{9}abc.*\n') /* Result: y */;
select regexp_like('a','^(a)?a$') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('a','^(a)?(?(1)a|b)+$') /* Result: n */;
select regexp_like('aaaaaa','^(a\\1?)(a\\1?)(a\\2?)(a\\3?)$') /* Result: y */;
select regexp_like('aaaaaa','^(a\\1?){4}$') /* Result: y */;
select regexp_like('x1','^(0+)?(?:x(1))?') /* Result: y */;
select regexp_like('012cxx0190','^([0-9a-fA-F]+)(?:x([0-9a-fA-F]+)?)(?:x([0-9a-fA-F]+))?') /* Result: y */;
select regexp_like('bbbac','^(b+?|a){1,2}c') /* Result: y */;
select regexp_like('bbbbac','^(b+?|a){1,2}c') /* Result: y */;
select regexp_like('cd. (A. Tw)','\\((\\w\\. \\w+)\\)') /* Result: y */;
select regexp_like('aaaacccc','((?:aaaa|bbbb)cccc)?') /* Result: y */;
select regexp_like('bbbbcccc','((?:aaaa|bbbb)cccc)?') /* Result: y */;
select regexp_like('a','(a)?(a)+') /* Result: y */;
select regexp_like('ab','(ab)?(ab)+') /* Result: y */;
select regexp_like('abc','(abc)?(abc)+') /* Result: y */;
select regexp_like('a\nb\n','(?m)b\\s^') /* Result: n */;
select regexp_like('a','\\ba') /* Result: y */;
# ?? Not supported
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('ab','^(a(??{"(?!)"})|(a)(?{1}))b') /* Result: yi */;
select regexp_like('AbCd','ab(?i)cd') /* Result: n */;
select regexp_like('abCd','ab(?i)cd') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('CD','(A|B)*(?(1)(CD)|(CD))') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('ABCD','(A|B)*(?(1)(CD)|(CD))') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('CD','(A|B)*?(?(1)(CD)|(CD))') /* Result: y */;
# Not implemented
--error ER_REGEXP_UNIMPLEMENTED
select regexp_like('ABCD','(A|B)*?(?(1)(CD)|(CD))') /* Result: y */;
select regexp_like('Oo','(?i)^(o)(?!.*\\1)') /* Result: n */;
select regexp_like('abc12bc','(.*)\\d+\\1') /* Result: y */;
select regexp_like('foo\n bar','(?m:(foo\\s*$))') /* Result: y */;
select regexp_like('abcd','(.*)c') /* Result: y */;
select regexp_like('abcd','(.*)(?=c)') /* Result: y */;
select regexp_like('abcd','(.*)(?=c)c') /* Result: yB */;
select regexp_like('abcd','(.*)(?=b|c)') /* Result: y */;
select regexp_like('abcd','(.*)(?=b|c)c') /* Result: y */;
select regexp_like('abcd','(.*)(?=c|b)') /* Result: y */;
select regexp_like('abcd','(.*)(?=c|b)c') /* Result: y */;
select regexp_like('abcd','(.*)(?=[bc])') /* Result: y */;
select regexp_like('abcd','(.*)(?=[bc])c') /* Result: yB */;
select regexp_like('abcd','(.*)(?<=b)') /* Result: y */;
select regexp_like('abcd','(.*)(?<=b)c') /* Result: y */;
select regexp_like('abcd','(.*)(?<=b|c)') /* Result: y */;
select regexp_like('abcd','(.*)(?<=b|c)c') /* Result: y */;
select regexp_like('abcd','(.*)(?<=c|b)') /* Result: y */;
select regexp_like('abcd','(.*)(?<=c|b)c') /* Result: y */;
select regexp_like('abcd','(.*)(?<=[bc])') /* Result: y */;
select regexp_like('abcd','(.*)(?<=[bc])c') /* Result: y */;
select regexp_like('abcd','(.*?)c') /* Result: y */;
select regexp_like('abcd','(.*?)(?=c)') /* Result: y */;
select regexp_like('abcd','(.*?)(?=c)c') /* Result: yB */;
select regexp_like('abcd','(.*?)(?=b|c)') /* Result: y */;
select regexp_like('abcd','(.*?)(?=b|c)c') /* Result: y */;
select regexp_like('abcd','(.*?)(?=c|b)') /* Result: y */;
select regexp_like('abcd','(.*?)(?=c|b)c') /* Result: y */;
select regexp_like('abcd','(.*?)(?=[bc])') /* Result: y */;
select regexp_like('abcd','(.*?)(?=[bc])c') /* Result: yB */;
select regexp_like('abcd','(.*?)(?<=b)') /* Result: y */;
select regexp_like('abcd','(.*?)(?<=b)c') /* Result: y */;
select regexp_like('abcd','(.*?)(?<=b|c)') /* Result: y */;
select regexp_like('abcd','(.*?)(?<=b|c)c') /* Result: y */;
select regexp_like('abcd','(.*?)(?<=c|b)') /* Result: y */;
select regexp_like('abcd','(.*?)(?<=c|b)c') /* Result: y */;
select regexp_like('abcd','(.*?)(?<=[bc])') /* Result: y */;
select regexp_like('abcd','(.*?)(?<=[bc])c') /* Result: y */;
select regexp_like('2','2(]*)?$\\1') /* Result: y */;
# ?? not supported
--error ER_REGEXP_RULE_SYNTAX
select regexp_like('x','(??{})') /* Result: yi */;
