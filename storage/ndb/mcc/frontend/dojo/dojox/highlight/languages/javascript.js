//>>built
define("dojox/highlight/languages/javascript",["dojox/main","../_base"],function(_1){
var dh=_1.highlight,_2=dh.constants;
dh.languages.javascript={defaultMode:{lexems:[_2.UNDERSCORE_IDENT_RE],contains:["string","comment","number","regexp","function"],keywords:{"keyword":{"in":1,"if":1,"for":1,"while":1,"finally":1,"var":1,"new":1,"function":1,"do":1,"return":1,"void":1,"else":1,"break":1,"catch":1,"instanceof":1,"with":1,"throw":1,"case":1,"default":1,"try":1,"this":1,"switch":1,"continue":1,"typeof":1,"delete":1},"literal":{"true":1,"false":1,"null":1}}},modes:[_2.C_LINE_COMMENT_MODE,_2.C_BLOCK_COMMENT_MODE,_2.C_NUMBER_MODE,_2.APOS_STRING_MODE,_2.QUOTE_STRING_MODE,_2.BACKSLASH_ESCAPE,{className:"regexp",begin:"/.*?[^\\\\/]/[gim]*",end:"^"},{className:"function",begin:"function\\b",end:"{",lexems:[_2.UNDERSCORE_IDENT_RE],keywords:{"function":1},contains:["title","params"]},{className:"title",begin:_2.UNDERSCORE_IDENT_RE,end:"^"},{className:"params",begin:"\\(",end:"\\)",contains:["string","comment"]}]};
return dh.languages.javascript;
});
