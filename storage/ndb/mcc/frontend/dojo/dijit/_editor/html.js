//>>built
define("dijit/_editor/html",["dojo/_base/lang","dojo/_base/sniff",".."],function(_1,_2,_3){
_1.getObject("_editor",true,_3);
_3._editor.escapeXml=function(_4,_5){
_4=_4.replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;").replace(/"/gm,"&quot;");
if(!_5){
_4=_4.replace(/'/gm,"&#39;");
}
return _4;
};
_3._editor.getNodeHtml=function(_6){
var _7;
switch(_6.nodeType){
case 1:
var _8=_6.nodeName.toLowerCase();
if(!_8||_8.charAt(0)=="/"){
return "";
}
_7="<"+_8;
var _9=[];
var _a;
if(_2("ie")&&_6.outerHTML){
var s=_6.outerHTML;
s=s.substr(0,s.indexOf(">")).replace(/(['"])[^"']*\1/g,"");
var _b=/(\b\w+)\s?=/g;
var m,_c;
while((m=_b.exec(s))){
_c=m[1];
if(_c.substr(0,3)!="_dj"){
if(_c=="src"||_c=="href"){
if(_6.getAttribute("_djrealurl")){
_9.push([_c,_6.getAttribute("_djrealurl")]);
continue;
}
}
var _d,_e;
switch(_c){
case "style":
_d=_6.style.cssText.toLowerCase();
break;
case "class":
_d=_6.className;
break;
case "width":
if(_8==="img"){
_e=/width=(\S+)/i.exec(s);
if(_e){
_d=_e[1];
}
break;
}
case "height":
if(_8==="img"){
_e=/height=(\S+)/i.exec(s);
if(_e){
_d=_e[1];
}
break;
}
default:
_d=_6.getAttribute(_c);
}
if(_d!=null){
_9.push([_c,_d.toString()]);
}
}
}
}else{
var i=0;
while((_a=_6.attributes[i++])){
var n=_a.name;
if(n.substr(0,3)!="_dj"){
var v=_a.value;
if(n=="src"||n=="href"){
if(_6.getAttribute("_djrealurl")){
v=_6.getAttribute("_djrealurl");
}
}
_9.push([n,v]);
}
}
}
_9.sort(function(a,b){
return a[0]<b[0]?-1:(a[0]==b[0]?0:1);
});
var j=0;
while((_a=_9[j++])){
_7+=" "+_a[0]+"=\""+(_1.isString(_a[1])?_3._editor.escapeXml(_a[1],true):_a[1])+"\"";
}
if(_8==="script"){
_7+=">"+_6.innerHTML+"</"+_8+">";
}else{
if(_6.childNodes.length){
_7+=">"+_3._editor.getChildrenHtml(_6)+"</"+_8+">";
}else{
switch(_8){
case "br":
case "hr":
case "img":
case "input":
case "base":
case "meta":
case "area":
case "basefont":
_7+=" />";
break;
default:
_7+="></"+_8+">";
}
}
}
break;
case 4:
case 3:
_7=_3._editor.escapeXml(_6.nodeValue,true);
break;
case 8:
_7="<!--"+_3._editor.escapeXml(_6.nodeValue,true)+"-->";
break;
default:
_7="<!-- Element not recognized - Type: "+_6.nodeType+" Name: "+_6.nodeName+"-->";
}
return _7;
};
_3._editor.getChildrenHtml=function(_f){
var out="";
if(!_f){
return out;
}
var _10=_f["childNodes"]||_f;
var _11=!_2("ie")||_10!==_f;
var _12,i=0;
while((_12=_10[i++])){
if(!_11||_12.parentNode==_f){
out+=_3._editor.getNodeHtml(_12);
}
}
return out;
};
return _3._editor;
});
