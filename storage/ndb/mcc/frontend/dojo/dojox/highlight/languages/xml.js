//>>built
define("dojox/highlight/languages/xml",["../_base"],function(dh){
var _1={className:"comment",begin:"<!--",end:"-->"};
var _2={className:"attribute",begin:" [a-zA-Z-]+\\s*=\\s*",end:"^",contains:["value"]};
var _3={className:"value",begin:"\"",end:"\""};
var _4=dh.constants;
dh.languages.xml={defaultMode:{contains:["pi","comment","cdata","tag"]},case_insensitive:true,modes:[{className:"pi",begin:"<\\?",end:"\\?>",relevance:10},_1,{className:"cdata",begin:"<\\!\\[CDATA\\[",end:"\\]\\]>"},{className:"tag",begin:"</?",end:">",contains:["title","tag_internal"],relevance:1.5},{className:"title",begin:"[A-Za-z:_][A-Za-z0-9\\._:-]+",end:"^",relevance:0},{className:"tag_internal",begin:"^",endsWithParent:true,contains:["attribute"],relevance:0,illegal:"[\\+\\.]"},_2,_3],XML_COMMENT:_1,XML_ATTR:_2,XML_VALUE:_3};
return dh.languages.xml;
});
