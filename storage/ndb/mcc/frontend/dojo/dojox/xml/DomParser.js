//>>built
define("dojox/xml/DomParser",["dojo/_base/kernel","dojo/_base/array"],function(_1){
_1.getObject("xml",true,dojox);
dojox.xml.DomParser=new (function(){
var _2={ELEMENT:1,ATTRIBUTE:2,TEXT:3,CDATA_SECTION:4,PROCESSING_INSTRUCTION:7,COMMENT:8,DOCUMENT:9};
var _3=/<([^>\/\s+]*)([^>]*)>([^<]*)/g;
var _4=/([^=]*)=(("([^"]*)")|('([^']*)'))/g;
var _5=/<!ENTITY\s+([^"]*)\s+"([^"]*)">/g;
var _6=/<!\[CDATA\[([\u0001-\uFFFF]*?)\]\]>/g;
var _7=/<!--([\u0001-\uFFFF]*?)-->/g;
var _8=/^\s+|\s+$/g;
var _9=/\s+/g;
var _a=/\&gt;/g;
var _b=/\&lt;/g;
var _c=/\&quot;/g;
var _d=/\&apos;/g;
var _e=/\&amp;/g;
var _f="_def_";
function _10(){
return new (function(){
var all={};
this.nodeType=_2.DOCUMENT;
this.nodeName="#document";
this.namespaces={};
this._nsPaths={};
this.childNodes=[];
this.documentElement=null;
this._add=function(obj){
if(typeof (obj.id)!="undefined"){
all[obj.id]=obj;
}
};
this._remove=function(id){
if(all[id]){
delete all[id];
}
};
this.byId=this.getElementById=function(id){
return all[id];
};
this.byName=this.getElementsByTagName=_11;
this.byNameNS=this.getElementsByTagNameNS=_12;
this.childrenByName=_13;
this.childrenByNameNS=_14;
})();
};
function _11(_15){
function _16(_17,_18,arr){
_1.forEach(_17.childNodes,function(c){
if(c.nodeType==_2.ELEMENT){
if(_18=="*"){
arr.push(c);
}else{
if(c.nodeName==_18){
arr.push(c);
}
}
_16(c,_18,arr);
}
});
};
var a=[];
_16(this,_15,a);
return a;
};
function _12(_19,ns){
function _1a(_1b,_1c,ns,arr){
_1.forEach(_1b.childNodes,function(c){
if(c.nodeType==_2.ELEMENT){
if(_1c=="*"&&c.ownerDocument._nsPaths[ns]==c.namespace){
arr.push(c);
}else{
if(c.localName==_1c&&c.ownerDocument._nsPaths[ns]==c.namespace){
arr.push(c);
}
}
_1a(c,_1c,ns,arr);
}
});
};
if(!ns){
ns=_f;
}
var a=[];
_1a(this,_19,ns,a);
return a;
};
function _13(_1d){
var a=[];
_1.forEach(this.childNodes,function(c){
if(c.nodeType==_2.ELEMENT){
if(_1d=="*"){
a.push(c);
}else{
if(c.nodeName==_1d){
a.push(c);
}
}
}
});
return a;
};
function _14(_1e,ns){
var a=[];
_1.forEach(this.childNodes,function(c){
if(c.nodeType==_2.ELEMENT){
if(_1e=="*"&&c.ownerDocument._nsPaths[ns]==c.namespace){
a.push(c);
}else{
if(c.localName==_1e&&c.ownerDocument._nsPaths[ns]==c.namespace){
a.push(c);
}
}
}
});
return a;
};
function _1f(v){
return {nodeType:_2.TEXT,nodeName:"#text",nodeValue:v.replace(_9," ").replace(_a,">").replace(_b,"<").replace(_d,"'").replace(_c,"\"").replace(_e,"&")};
};
function _20(_21){
for(var i=0;i<this.attributes.length;i++){
if(this.attributes[i].nodeName==_21){
return this.attributes[i].nodeValue;
}
}
return null;
};
function _22(_23,ns){
for(var i=0;i<this.attributes.length;i++){
if(this.ownerDocument._nsPaths[ns]==this.attributes[i].namespace&&this.attributes[i].localName==_23){
return this.attributes[i].nodeValue;
}
}
return null;
};
function _24(_25,val){
var old=null;
for(var i=0;i<this.attributes.length;i++){
if(this.attributes[i].nodeName==_25){
old=this.attributes[i].nodeValue;
this.attributes[i].nodeValue=val;
break;
}
}
if(_25=="id"){
if(old!=null){
this.ownerDocument._remove(old);
}
this.ownerDocument._add(this);
}
};
function _26(_27,val,ns){
for(var i=0;i<this.attributes.length;i++){
if(this.ownerDocument._nsPaths[ns]==this.attributes[i].namespace&&this.attributes[i].localName==_27){
this.attributes[i].nodeValue=val;
return;
}
}
};
function _28(){
var p=this.parentNode;
if(p){
for(var i=0;i<p.childNodes.length;i++){
if(p.childNodes[i]==this&&i>0){
return p.childNodes[i-1];
}
}
}
return null;
};
function _29(){
var p=this.parentNode;
if(p){
for(var i=0;i<p.childNodes.length;i++){
if(p.childNodes[i]==this&&(i+1)<p.childNodes.length){
return p.childNodes[i+1];
}
}
}
return null;
};
this.parse=function(str){
var _2a=_10();
if(str==null){
return _2a;
}
if(str.length==0){
return _2a;
}
if(str.indexOf("<!ENTITY")>0){
var _2b,eRe=[];
if(_5.test(str)){
_5.lastIndex=0;
while((_2b=_5.exec(str))!=null){
eRe.push({entity:"&"+_2b[1].replace(_8,"")+";",expression:_2b[2]});
}
for(var i=0;i<eRe.length;i++){
str=str.replace(new RegExp(eRe[i].entity,"g"),eRe[i].expression);
}
}
}
var _2c=[],_2d;
while((_2d=_6.exec(str))!=null){
_2c.push(_2d[1]);
}
for(var i=0;i<_2c.length;i++){
str=str.replace(_2c[i],i);
}
var _2e=[],_2f;
while((_2f=_7.exec(str))!=null){
_2e.push(_2f[1]);
}
for(i=0;i<_2e.length;i++){
str=str.replace(_2e[i],i);
}
var res,obj=_2a;
while((res=_3.exec(str))!=null){
if(res[2].charAt(0)=="/"&&res[2].replace(_8,"").length>1){
if(obj.parentNode){
obj=obj.parentNode;
}
var _30=(res[3]||"").replace(_8,"");
if(_30.length>0){
obj.childNodes.push(_1f(_30));
}
}else{
if(res[1].length>0){
if(res[1].charAt(0)=="?"){
var _31=res[1].substr(1);
var _32=res[2].substr(0,res[2].length-2);
obj.childNodes.push({nodeType:_2.PROCESSING_INSTRUCTION,nodeName:_31,nodeValue:_32});
}else{
if(res[1].charAt(0)=="!"){
if(res[1].indexOf("![CDATA[")==0){
var val=parseInt(res[1].replace("![CDATA[","").replace("]]",""));
obj.childNodes.push({nodeType:_2.CDATA_SECTION,nodeName:"#cdata-section",nodeValue:_2c[val]});
}else{
if(res[1].substr(0,3)=="!--"){
var val=parseInt(res[1].replace("!--","").replace("--",""));
obj.childNodes.push({nodeType:_2.COMMENT,nodeName:"#comment",nodeValue:_2e[val]});
}
}
}else{
var _31=res[1].replace(_8,"");
var o={nodeType:_2.ELEMENT,nodeName:_31,localName:_31,namespace:_f,ownerDocument:_2a,attributes:[],parentNode:null,childNodes:[]};
if(_31.indexOf(":")>-1){
var t=_31.split(":");
o.namespace=t[0];
o.localName=t[1];
}
o.byName=o.getElementsByTagName=_11;
o.byNameNS=o.getElementsByTagNameNS=_12;
o.childrenByName=_13;
o.childrenByNameNS=_14;
o.getAttribute=_20;
o.getAttributeNS=_22;
o.setAttribute=_24;
o.setAttributeNS=_26;
o.previous=o.previousSibling=_28;
o.next=o.nextSibling=_29;
var _33;
while((_33=_4.exec(res[2]))!=null){
if(_33.length>0){
var _31=_33[1].replace(_8,"");
var val=(_33[4]||_33[6]||"").replace(_9," ").replace(_a,">").replace(_b,"<").replace(_d,"'").replace(_c,"\"").replace(_e,"&");
if(_31.indexOf("xmlns")==0){
if(_31.indexOf(":")>0){
var ns=_31.split(":");
_2a.namespaces[ns[1]]=val;
_2a._nsPaths[val]=ns[1];
}else{
_2a.namespaces[_f]=val;
_2a._nsPaths[val]=_f;
}
}else{
var ln=_31;
var ns=_f;
if(_31.indexOf(":")>0){
var t=_31.split(":");
ln=t[1];
ns=t[0];
}
o.attributes.push({nodeType:_2.ATTRIBUTE,nodeName:_31,localName:ln,namespace:ns,nodeValue:val});
if(ln=="id"){
o.id=val;
}
}
}
}
_2a._add(o);
if(obj){
obj.childNodes.push(o);
o.parentNode=obj;
if(res[2].charAt(res[2].length-1)!="/"){
obj=o;
}
}
var _30=res[3];
if(_30.length>0){
obj.childNodes.push(_1f(_30));
}
}
}
}
}
}
for(var i=0;i<_2a.childNodes.length;i++){
var e=_2a.childNodes[i];
if(e.nodeType==_2.ELEMENT){
_2a.documentElement=e;
break;
}
}
return _2a;
};
})();
return dojox.xml.DomParser;
});
