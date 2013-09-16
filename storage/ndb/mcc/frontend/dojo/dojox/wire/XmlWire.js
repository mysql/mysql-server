//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/xml/parser,dojox/wire/Wire"],function(_1,_2,_3){
_2.provide("dojox.wire.XmlWire");
_2.require("dojox.xml.parser");
_2.require("dojox.wire.Wire");
_2.declare("dojox.wire.XmlWire",_3.wire.Wire,{_wireClass:"dojox.wire.XmlWire",constructor:function(_4){
},_getValue:function(_5){
if(!_5||!this.path){
return _5;
}
var _6=_5;
var _7=this.path;
var i;
if(_7.charAt(0)=="/"){
i=_7.indexOf("/",1);
_7=_7.substring(i+1);
}
var _8=_7.split("/");
var _9=_8.length-1;
for(i=0;i<_9;i++){
_6=this._getChildNode(_6,_8[i]);
if(!_6){
return undefined;
}
}
var _a=this._getNodeValue(_6,_8[_9]);
return _a;
},_setValue:function(_b,_c){
if(!this.path){
return _b;
}
var _d=_b;
var _e=this._getDocument(_d);
var _f=this.path;
var i;
if(_f.charAt(0)=="/"){
i=_f.indexOf("/",1);
if(!_d){
var _10=_f.substring(1,i);
_d=_e.createElement(_10);
_b=_d;
}
_f=_f.substring(i+1);
}else{
if(!_d){
return undefined;
}
}
var _11=_f.split("/");
var _12=_11.length-1;
for(i=0;i<_12;i++){
var _13=this._getChildNode(_d,_11[i]);
if(!_13){
_13=_e.createElement(_11[i]);
_d.appendChild(_13);
}
_d=_13;
}
this._setNodeValue(_d,_11[_12],_c);
return _b;
},_getNodeValue:function(_14,exp){
var _15=undefined;
if(exp.charAt(0)=="@"){
var _16=exp.substring(1);
_15=_14.getAttribute(_16);
}else{
if(exp=="text()"){
var _17=_14.firstChild;
if(_17){
_15=_17.nodeValue;
}
}else{
_15=[];
for(var i=0;i<_14.childNodes.length;i++){
var _18=_14.childNodes[i];
if(_18.nodeType===1&&_18.nodeName==exp){
_15.push(_18);
}
}
}
}
return _15;
},_setNodeValue:function(_19,exp,_1a){
if(exp.charAt(0)=="@"){
var _1b=exp.substring(1);
if(_1a){
_19.setAttribute(_1b,_1a);
}else{
_19.removeAttribute(_1b);
}
}else{
if(exp=="text()"){
while(_19.firstChild){
_19.removeChild(_19.firstChild);
}
if(_1a){
var _1c=this._getDocument(_19).createTextNode(_1a);
_19.appendChild(_1c);
}
}
}
},_getChildNode:function(_1d,_1e){
var _1f=1;
var i1=_1e.indexOf("[");
if(i1>=0){
var i2=_1e.indexOf("]");
_1f=_1e.substring(i1+1,i2);
_1e=_1e.substring(0,i1);
}
var _20=1;
for(var i=0;i<_1d.childNodes.length;i++){
var _21=_1d.childNodes[i];
if(_21.nodeType===1&&_21.nodeName==_1e){
if(_20==_1f){
return _21;
}
_20++;
}
}
return null;
},_getDocument:function(_22){
if(_22){
return (_22.nodeType==9?_22:_22.ownerDocument);
}else{
return _3.xml.parser.parse();
}
}});
});
