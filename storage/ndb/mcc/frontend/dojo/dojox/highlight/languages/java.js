//>>built
define("dojox/highlight/languages/java",["../_base"],function(dh){
var _1=dh.constants;
var _2={"false":1,"int":1,"float":1,"while":1,"private":1,"char":1,"catch":1,"abstract":1,"assert":1,"const":1,"byte":1,"for":1,"final":1,"finally":1,"implements":1,"import":1,"extends":1,"long":1,"throw":1,"instanceof":2,"static":1,"protected":1,"boolean":1,"interface":2,"native":1,"if":1,"public":1,"new":1,"do":1,"return":1,"goto":1,"package":2,"void":2,"short":1,"else":1,"break":1,"new":1,"strictfp":1,"super":1,"true":1,"class":1,"synchronized":1,"case":1,"this":1,"short":1,"throws":1,"transient":1,"double":1,"volatile":1,"try":1,"this":1,"switch":1,"continue":1};
dh.languages.java={defaultMode:{lexems:[_1.UNDERSCORE_IDENT_RE],illegal:"</",contains:["comment","string","number","function","block"],keywords:_2},modes:[_1.C_LINE_COMMENT_MODE,_1.C_BLOCK_COMMENT_MODE,_1.C_NUMBER_MODE,_1.QUOTE_STRING_MODE,_1.BACKSLASH_ESCAPE,{className:"string",begin:"'",end:"[^\\\\]'",illegal:"[^\\\\][^']"},{className:"function",begin:"\\(",end:"\\)",contains:["comment","number","string","function","block"],keywords:_2},{lexems:[_1.UNDERSCORE_IDENT_RE],className:"block",begin:"\\{",end:"\\}",contains:["comment","string","number","function","block"],keywords:_2}]};
return dh.languages.java;
});
