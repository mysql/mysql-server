//>>built
define("dojox/highlight/languages/xml",["dojox/main","../_base"],function(_1){
var _2={className:"comment",begin:"<!--",end:"-->"};
var _3={className:"attribute",begin:" [a-zA-Z-]+\\s*=\\s*",end:"^",contains:["value"]};
var _4={className:"value",begin:"\"",end:"\""};
var dh=_1.highlight,_5=dh.constants;
dh.languages.xml={defaultMode:{contains:["pi","comment","cdata","tag"]},case_insensitive:true,modes:[{className:"pi",begin:"<\\?",end:"\\?>",relevance:10},_2,{className:"cdata",begin:"<\\!\\[CDATA\\[",end:"\\]\\]>"},{className:"tag",begin:"</?",end:">",contains:["title","tag_internal"],relevance:1.5},{className:"title",begin:"[A-Za-z:_][A-Za-z0-9\\._:-]+",end:"^",relevance:0},{className:"tag_internal",begin:"^",endsWithParent:true,contains:["attribute"],relevance:0,illegal:"[\\+\\.]"},_3,_4],XML_COMMENT:_2,XML_ATTR:_3,XML_VALUE:_4};
return dh.languages.xml;
});
