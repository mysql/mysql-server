/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom-construct",["exports","./_base/kernel","./_base/sniff","./_base/window","./dom","./dom-attr","./on"],function(_1,_2,_3,_4,_5,_6,on){
var _7={option:["select"],tbody:["table"],thead:["table"],tfoot:["table"],tr:["table","tbody"],td:["table","tbody","tr"],th:["table","thead","tr"],legend:["fieldset"],caption:["table"],colgroup:["table"],col:["table","colgroup"],li:["ul"]},_8=/<\s*([\w\:]+)/,_9={},_a=0,_b="__"+_2._scopeName+"ToDomId";
for(var _c in _7){
if(_7.hasOwnProperty(_c)){
var tw=_7[_c];
tw.pre=_c=="option"?"<select multiple=\"multiple\">":"<"+tw.join("><")+">";
tw.post="</"+tw.reverse().join("></")+">";
}
}
function _d(_e,_f){
var _10=_f.parentNode;
if(_10){
_10.insertBefore(_e,_f);
}
};
function _11(_12,ref){
var _13=ref.parentNode;
if(_13){
if(_13.lastChild==ref){
_13.appendChild(_12);
}else{
_13.insertBefore(_12,ref.nextSibling);
}
}
};
var _14=null,_15;
on(window,"unload",function(){
_14=null;
});
_1.toDom=function toDom(_16,doc){
doc=doc||_4.doc;
var _17=doc[_b];
if(!_17){
doc[_b]=_17=++_a+"";
_9[_17]=doc.createElement("div");
}
_16+="";
var _18=_16.match(_8),tag=_18?_18[1].toLowerCase():"",_19=_9[_17],_1a,i,fc,df;
if(_18&&_7[tag]){
_1a=_7[tag];
_19.innerHTML=_1a.pre+_16+_1a.post;
for(i=_1a.length;i;--i){
_19=_19.firstChild;
}
}else{
_19.innerHTML=_16;
}
if(_19.childNodes.length==1){
return _19.removeChild(_19.firstChild);
}
df=doc.createDocumentFragment();
while(fc=_19.firstChild){
df.appendChild(fc);
}
return df;
};
_1.place=function place(_1b,_1c,_1d){
_1c=_5.byId(_1c);
if(typeof _1b=="string"){
_1b=/^\s*</.test(_1b)?_1.toDom(_1b,_1c.ownerDocument):_5.byId(_1b);
}
if(typeof _1d=="number"){
var cn=_1c.childNodes;
if(!cn.length||cn.length<=_1d){
_1c.appendChild(_1b);
}else{
_d(_1b,cn[_1d<0?0:_1d]);
}
}else{
switch(_1d){
case "before":
_d(_1b,_1c);
break;
case "after":
_11(_1b,_1c);
break;
case "replace":
_1c.parentNode.replaceChild(_1b,_1c);
break;
case "only":
_1.empty(_1c);
_1c.appendChild(_1b);
break;
case "first":
if(_1c.firstChild){
_d(_1b,_1c.firstChild);
break;
}
default:
_1c.appendChild(_1b);
}
}
return _1b;
};
_1.create=function create(tag,_1e,_1f,pos){
var doc=_4.doc;
if(_1f){
_1f=_5.byId(_1f);
doc=_1f.ownerDocument;
}
if(typeof tag=="string"){
tag=doc.createElement(tag);
}
if(_1e){
_6.set(tag,_1e);
}
if(_1f){
_1.place(tag,_1f,pos);
}
return tag;
};
_1.empty=_3("ie")?function(_20){
_20=_5.byId(_20);
for(var c;c=_20.lastChild;){
_1.destroy(c);
}
}:function(_21){
_5.byId(_21).innerHTML="";
};
_1.destroy=function destroy(_22){
_22=_5.byId(_22);
try{
var doc=_22.ownerDocument;
if(!_14||_15!=doc){
_14=doc.createElement("div");
_15=doc;
}
_14.appendChild(_22.parentNode?_22.parentNode.removeChild(_22):_22);
_14.innerHTML="";
}
catch(e){
}
};
});
