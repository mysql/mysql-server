//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/xml/parser,dojox/wire/Wire"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.util");
_2.require("dojox.xml.parser");
_2.require("dojox.wire.Wire");
_3.wire.ml._getValue=function(_4,_5){
if(!_4){
return undefined;
}
var _6=undefined;
if(_5&&_4.length>=9&&_4.substring(0,9)=="arguments"){
_6=_4.substring(9);
return new _3.wire.Wire({property:_6}).getValue(_5);
}
var i=_4.indexOf(".");
if(i>=0){
_6=_4.substring(i+1);
_4=_4.substring(0,i);
}
var _7=(_1.byId(_4)||_2.byId(_4)||_2.getObject(_4));
if(!_7){
return undefined;
}
if(!_6){
return _7;
}else{
return new _3.wire.Wire({object:_7,property:_6}).getValue();
}
};
_3.wire.ml._setValue=function(_8,_9){
if(!_8){
return;
}
var i=_8.indexOf(".");
if(i<0){
return;
}
var _a=this._getValue(_8.substring(0,i));
if(!_a){
return;
}
var _b=_8.substring(i+1);
var _c=new _3.wire.Wire({object:_a,property:_b}).setValue(_9);
};
_2.declare("dojox.wire.ml.XmlElement",null,{constructor:function(_d){
if(_2.isString(_d)){
_d=this._getDocument().createElement(_d);
}
this.element=_d;
},getPropertyValue:function(_e){
var _f=undefined;
if(!this.element){
return _f;
}
if(!_e){
return _f;
}
if(_e.charAt(0)=="@"){
var _10=_e.substring(1);
_f=this.element.getAttribute(_10);
}else{
if(_e=="text()"){
var _11=this.element.firstChild;
if(_11){
_f=_11.nodeValue;
}
}else{
var _12=[];
for(var i=0;i<this.element.childNodes.length;i++){
var _13=this.element.childNodes[i];
if(_13.nodeType===1&&_13.nodeName==_e){
_12.push(new _3.wire.ml.XmlElement(_13));
}
}
if(_12.length>0){
if(_12.length===1){
_f=_12[0];
}else{
_f=_12;
}
}
}
}
return _f;
},setPropertyValue:function(_14,_15){
var i;
var _16;
if(!this.element){
return;
}
if(!_14){
return;
}
if(_14.charAt(0)=="@"){
var _17=_14.substring(1);
if(_15){
this.element.setAttribute(_17,_15);
}else{
this.element.removeAttribute(_17);
}
}else{
if(_14=="text()"){
while(this.element.firstChild){
this.element.removeChild(this.element.firstChild);
}
if(_15){
_16=this._getDocument().createTextNode(_15);
this.element.appendChild(_16);
}
}else{
var _18=null;
var _19;
for(i=this.element.childNodes.length-1;i>=0;i--){
_19=this.element.childNodes[i];
if(_19.nodeType===1&&_19.nodeName==_14){
if(!_18){
_18=_19.nextSibling;
}
this.element.removeChild(_19);
}
}
if(_15){
if(_2.isArray(_15)){
for(i in _15){
var e=_15[i];
if(e.element){
this.element.insertBefore(e.element,_18);
}
}
}else{
if(_15 instanceof _3.wire.ml.XmlElement){
if(_15.element){
this.element.insertBefore(_15.element,_18);
}
}else{
_19=this._getDocument().createElement(_14);
_16=this._getDocument().createTextNode(_15);
_19.appendChild(_16);
this.element.insertBefore(_19,_18);
}
}
}
}
}
},toString:function(){
var s="";
if(this.element){
var _1a=this.element.firstChild;
if(_1a){
s=_1a.nodeValue;
}
}
return s;
},toObject:function(){
if(!this.element){
return null;
}
var _1b="";
var obj={};
var _1c=0;
var i;
for(i=0;i<this.element.childNodes.length;i++){
var _1d=this.element.childNodes[i];
if(_1d.nodeType===1){
_1c++;
var o=new _3.wire.ml.XmlElement(_1d).toObject();
var _1e=_1d.nodeName;
var p=obj[_1e];
if(!p){
obj[_1e]=o;
}else{
if(_2.isArray(p)){
p.push(o);
}else{
obj[_1e]=[p,o];
}
}
}else{
if(_1d.nodeType===3||_1d.nodeType===4){
_1b+=_1d.nodeValue;
}
}
}
var _1f=0;
if(this.element.nodeType===1){
_1f=this.element.attributes.length;
for(i=0;i<_1f;i++){
var _20=this.element.attributes[i];
obj["@"+_20.nodeName]=_20.nodeValue;
}
}
if(_1c===0){
if(_1f===0){
return _1b;
}
obj["text()"]=_1b;
}
return obj;
},_getDocument:function(){
if(this.element){
return (this.element.nodeType==9?this.element:this.element.ownerDocument);
}else{
return _3.xml.parser.parse();
}
}});
});
