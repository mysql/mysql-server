//>>built
define("dojox/xml/parser",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/window","dojo/_base/sniff"],function(_1){
_1.getObject("xml.parser",true,dojox);
dojox.xml.parser.parse=function(_2,_3){
var _4=_1.doc;
var _5;
_3=_3||"text/xml";
if(_2&&_1.trim(_2)&&"DOMParser" in _1.global){
var _6=new DOMParser();
_5=_6.parseFromString(_2,_3);
var de=_5.documentElement;
var _7="http://www.mozilla.org/newlayout/xml/parsererror.xml";
if(de.nodeName=="parsererror"&&de.namespaceURI==_7){
var _8=de.getElementsByTagNameNS(_7,"sourcetext")[0];
if(_8){
_8=_8.firstChild.data;
}
throw new Error("Error parsing text "+de.firstChild.data+" \n"+_8);
}
return _5;
}else{
if("ActiveXObject" in _1.global){
var ms=function(n){
return "MSXML"+n+".DOMDocument";
};
var dp=["Microsoft.XMLDOM",ms(6),ms(4),ms(3),ms(2)];
_1.some(dp,function(p){
try{
_5=new ActiveXObject(p);
}
catch(e){
return false;
}
return true;
});
if(_2&&_5){
_5.async=false;
_5.loadXML(_2);
var pe=_5.parseError;
if(pe.errorCode!==0){
throw new Error("Line: "+pe.line+"\n"+"Col: "+pe.linepos+"\n"+"Reason: "+pe.reason+"\n"+"Error Code: "+pe.errorCode+"\n"+"Source: "+pe.srcText);
}
}
if(_5){
return _5;
}
}else{
if(_4.implementation&&_4.implementation.createDocument){
if(_2&&_1.trim(_2)&&_4.createElement){
var _9=_4.createElement("xml");
_9.innerHTML=_2;
var _a=_4.implementation.createDocument("foo","",null);
_1.forEach(_9.childNodes,function(_b){
_a.importNode(_b,true);
});
return _a;
}else{
return _4.implementation.createDocument("","",null);
}
}
}
}
return null;
};
dojox.xml.parser.textContent=function(_c,_d){
if(arguments.length>1){
var _e=_c.ownerDocument||_1.doc;
dojox.xml.parser.replaceChildren(_c,_e.createTextNode(_d));
return _d;
}else{
if(_c.textContent!==undefined){
return _c.textContent;
}
var _f="";
if(_c){
_1.forEach(_c.childNodes,function(_10){
switch(_10.nodeType){
case 1:
case 5:
_f+=dojox.xml.parser.textContent(_10);
break;
case 3:
case 2:
case 4:
_f+=_10.nodeValue;
}
});
}
return _f;
}
};
dojox.xml.parser.replaceChildren=function(_11,_12){
var _13=[];
if(_1.isIE){
_1.forEach(_11.childNodes,function(_14){
_13.push(_14);
});
}
dojox.xml.parser.removeChildren(_11);
_1.forEach(_13,_1.destroy);
if(!_1.isArray(_12)){
_11.appendChild(_12);
}else{
_1.forEach(_12,function(_15){
_11.appendChild(_15);
});
}
};
dojox.xml.parser.removeChildren=function(_16){
var _17=_16.childNodes.length;
while(_16.hasChildNodes()){
_16.removeChild(_16.firstChild);
}
return _17;
};
dojox.xml.parser.innerXML=function(_18){
if(_18.innerXML){
return _18.innerXML;
}else{
if(_18.xml){
return _18.xml;
}else{
if(typeof XMLSerializer!="undefined"){
return (new XMLSerializer()).serializeToString(_18);
}
}
}
return null;
};
return dojox.xml.parser;
});
