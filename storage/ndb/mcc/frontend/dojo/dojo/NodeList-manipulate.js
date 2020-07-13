/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/NodeList-manipulate",["./query","./_base/lang","./_base/array","./dom-construct","./dom-attr","./NodeList-dom"],function(_1,_2,_3,_4,_5){
var _6=_1.NodeList;
function _7(_8){
while(_8.childNodes[0]&&_8.childNodes[0].nodeType==1){
_8=_8.childNodes[0];
}
return _8;
};
function _9(_a,_b){
if(typeof _a=="string"){
_a=_4.toDom(_a,(_b&&_b.ownerDocument));
if(_a.nodeType==11){
_a=_a.childNodes[0];
}
}else{
if(_a.nodeType==1&&_a.parentNode){
_a=_a.cloneNode(false);
}
}
return _a;
};
_2.extend(_6,{_placeMultiple:function(_c,_d){
var _e=typeof _c=="string"||_c.nodeType?_1(_c):_c;
var _f=[];
for(var i=0;i<_e.length;i++){
var _10=_e[i];
var _11=this.length;
for(var j=_11-1,_12;_12=this[j];j--){
if(i>0){
_12=this._cloneNode(_12);
_f.unshift(_12);
}
if(j==_11-1){
_4.place(_12,_10,_d);
}else{
_10.parentNode.insertBefore(_12,_10);
}
_10=_12;
}
}
if(_f.length){
_f.unshift(0);
_f.unshift(this.length-1);
Array.prototype.splice.apply(this,_f);
}
return this;
},innerHTML:function(_13){
if(arguments.length){
return this.addContent(_13,"only");
}else{
return this[0].innerHTML;
}
},text:function(_14){
if(arguments.length){
for(var i=0,_15;_15=this[i];i++){
if(_15.nodeType==1){
_5.set(_15,"textContent",_14);
}
}
return this;
}else{
var _16="";
for(i=0;_15=this[i];i++){
_16+=_5.get(_15,"textContent");
}
return _16;
}
},val:function(_17){
if(arguments.length){
var _18=_2.isArray(_17);
for(var _19=0,_1a;_1a=this[_19];_19++){
var _1b=_1a.nodeName.toUpperCase();
var _1c=_1a.type;
var _1d=_18?_17[_19]:_17;
if(_1b=="SELECT"){
var _1e=_1a.options;
for(var i=0;i<_1e.length;i++){
var opt=_1e[i];
if(_1a.multiple){
opt.selected=(_3.indexOf(_17,opt.value)!=-1);
}else{
opt.selected=(opt.value==_1d);
}
}
}else{
if(_1c=="checkbox"||_1c=="radio"){
_1a.checked=(_1a.value==_1d);
}else{
_1a.value=_1d;
}
}
}
return this;
}else{
_1a=this[0];
if(!_1a||_1a.nodeType!=1){
return undefined;
}
_17=_1a.value||"";
if(_1a.nodeName.toUpperCase()=="SELECT"&&_1a.multiple){
_17=[];
_1e=_1a.options;
for(i=0;i<_1e.length;i++){
opt=_1e[i];
if(opt.selected){
_17.push(opt.value);
}
}
if(!_17.length){
_17=null;
}
}
return _17;
}
},append:function(_1f){
return this.addContent(_1f,"last");
},appendTo:function(_20){
return this._placeMultiple(_20,"last");
},prepend:function(_21){
return this.addContent(_21,"first");
},prependTo:function(_22){
return this._placeMultiple(_22,"first");
},after:function(_23){
return this.addContent(_23,"after");
},insertAfter:function(_24){
return this._placeMultiple(_24,"after");
},before:function(_25){
return this.addContent(_25,"before");
},insertBefore:function(_26){
return this._placeMultiple(_26,"before");
},remove:_6.prototype.orphan,wrap:function(_27){
if(this[0]){
_27=_9(_27,this[0]);
for(var i=0,_28;_28=this[i];i++){
var _29=this._cloneNode(_27);
if(_28.parentNode){
_28.parentNode.replaceChild(_29,_28);
}
var _2a=_7(_29);
_2a.appendChild(_28);
}
}
return this;
},wrapAll:function(_2b){
if(this[0]){
_2b=_9(_2b,this[0]);
this[0].parentNode.replaceChild(_2b,this[0]);
var _2c=_7(_2b);
for(var i=0,_2d;_2d=this[i];i++){
_2c.appendChild(_2d);
}
}
return this;
},wrapInner:function(_2e){
if(this[0]){
_2e=_9(_2e,this[0]);
for(var i=0;i<this.length;i++){
var _2f=this._cloneNode(_2e);
this._wrap(_2._toArray(this[i].childNodes),null,this._NodeListCtor).wrapAll(_2f);
}
}
return this;
},replaceWith:function(_30){
_30=this._normalize(_30,this[0]);
for(var i=0,_31;_31=this[i];i++){
this._place(_30,_31,"before",i>0);
_31.parentNode.removeChild(_31);
}
return this;
},replaceAll:function(_32){
var nl=_1(_32);
var _33=this._normalize(this,this[0]);
for(var i=0,_34;_34=nl[i];i++){
this._place(_33,_34,"before",i>0);
_34.parentNode.removeChild(_34);
}
return this;
},clone:function(){
var ary=[];
for(var i=0;i<this.length;i++){
ary.push(this._cloneNode(this[i]));
}
return this._wrap(ary,this,this._NodeListCtor);
}});
if(!_6.prototype.html){
_6.prototype.html=_6.prototype.innerHTML;
}
return _6;
});
