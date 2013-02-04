//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/string/Builder,dojox/encoding/base64"],function(_1,_2,_3){
_2.provide("dojox.xmpp.util");
_2.require("dojox.string.Builder");
_2.require("dojox.encoding.base64");
_3.xmpp.util.xmlEncode=function(_4){
if(_4){
_4=_4.replace("&","&amp;").replace(">","&gt;").replace("<","&lt;").replace("'","&apos;").replace("\"","&quot;");
}
return _4;
};
_3.xmpp.util.encodeJid=function(_5){
var _6=new _3.string.Builder();
for(var i=0;i<_5.length;i++){
var ch=_5.charAt(i);
var _7=ch;
switch(ch){
case " ":
_7="\\20";
break;
case "\"":
_7="\\22";
break;
case "#":
_7="\\23";
break;
case "&":
_7="\\26";
break;
case "'":
_7="\\27";
break;
case "/":
_7="\\2f";
break;
case ":":
_7="\\3a";
break;
case "<":
_7="\\3c";
break;
case ">":
_7="\\3e";
break;
}
_6.append(_7);
}
return _6.toString();
};
_3.xmpp.util.decodeJid=function(_8){
_8=_8.replace(/\\([23][02367acef])/g,function(_9){
switch(_9){
case "\\20":
return " ";
case "\\22":
return "\"";
case "\\23":
return "#";
case "\\26":
return "&";
case "\\27":
return "'";
case "\\2f":
return "/";
case "\\3a":
return ":";
case "\\3c":
return "<";
case "\\3e":
return ">";
}
return "ARG";
});
return _8;
};
_3.xmpp.util.createElement=function(_a,_b,_c){
var _d=new _3.string.Builder("<");
_d.append(_a+" ");
for(var _e in _b){
_d.append(_e+"=\"");
_d.append(_b[_e]);
_d.append("\" ");
}
if(_c){
_d.append("/>");
}else{
_d.append(">");
}
return _d.toString();
};
_3.xmpp.util.stripHtml=function(_f){
var re=/<[^>]*?>/gi;
for(var i=0;i<arguments.length;i++){
}
return _f.replace(re,"");
};
_3.xmpp.util.decodeHtmlEntities=function(str){
var ta=_2.doc.createElement("textarea");
ta.innerHTML=str.replace(/</g,"&lt;").replace(/>/g,"&gt;");
return ta.value;
};
_3.xmpp.util.htmlToPlain=function(str){
str=_3.xmpp.util.decodeHtmlEntities(str);
str=str.replace(/<br\s*[i\/]{0,1}>/gi,"\n");
str=_3.xmpp.util.stripHtml(str);
return str;
};
_3.xmpp.util.Base64={};
_3.xmpp.util.Base64.encode=function(_10){
var s2b=function(s){
var b=[];
for(var i=0;i<s.length;++i){
b.push(s.charCodeAt(i));
}
return b;
};
return _3.encoding.base64.encode(s2b(_10));
};
_3.xmpp.util.Base64.decode=function(_11){
var b2s=function(b){
var s=[];
_2.forEach(b,function(c){
s.push(String.fromCharCode(c));
});
return s.join("");
};
return b2s(_3.encoding.base64.decode(_11));
};
});
