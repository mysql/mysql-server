/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom-construct",["exports","./_base/kernel","./sniff","./_base/window","./dom","./dom-attr","./on"],function(_1,_2,_3,_4,_5,_6,on){
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
_1.toDom=function toDom(_14,doc){
doc=doc||_4.doc;
var _15=doc[_b];
if(!_15){
doc[_b]=_15=++_a+"";
_9[_15]=doc.createElement("div");
}
_14+="";
var _16=_14.match(_8),tag=_16?_16[1].toLowerCase():"",_17=_9[_15],_18,i,fc,df;
if(_16&&_7[tag]){
_18=_7[tag];
_17.innerHTML=_18.pre+_14+_18.post;
for(i=_18.length;i;--i){
_17=_17.firstChild;
}
}else{
_17.innerHTML=_14;
}
if(_17.childNodes.length==1){
return _17.removeChild(_17.firstChild);
}
df=doc.createDocumentFragment();
while((fc=_17.firstChild)){
df.appendChild(fc);
}
return df;
};
_1.place=function place(_19,_1a,_1b){
_1a=_5.byId(_1a);
if(typeof _19=="string"){
_19=/^\s*</.test(_19)?_1.toDom(_19,_1a.ownerDocument):_5.byId(_19);
}
if(typeof _1b=="number"){
var cn=_1a.childNodes;
if(!cn.length||cn.length<=_1b){
_1a.appendChild(_19);
}else{
_d(_19,cn[_1b<0?0:_1b]);
}
}else{
switch(_1b){
case "before":
_d(_19,_1a);
break;
case "after":
_11(_19,_1a);
break;
case "replace":
_1a.parentNode.replaceChild(_19,_1a);
break;
case "only":
_1.empty(_1a);
_1a.appendChild(_19);
break;
case "first":
if(_1a.firstChild){
_d(_19,_1a.firstChild);
break;
}
default:
_1a.appendChild(_19);
}
}
return _19;
};
_1.create=function create(tag,_1c,_1d,pos){
var doc=_4.doc;
if(_1d){
_1d=_5.byId(_1d);
doc=_1d.ownerDocument;
}
if(typeof tag=="string"){
tag=doc.createElement(tag);
}
if(_1c){
_6.set(tag,_1c);
}
if(_1d){
_1.place(tag,_1d,pos);
}
return tag;
};
function _1e(_1f){
if(_1f.canHaveChildren){
try{
_1f.innerHTML="";
return;
}
catch(e){
}
}
for(var c;c=_1f.lastChild;){
_20(c,_1f);
}
};
_1.empty=function empty(_21){
_1e(_5.byId(_21));
};
function _20(_22,_23){
if(_22.firstChild){
_1e(_22);
}
if(_23){
_3("ie")&&_23.canHaveChildren&&"removeNode" in _22?_22.removeNode(false):_23.removeChild(_22);
}
};
_1.destroy=function destroy(_24){
_24=_5.byId(_24);
if(!_24){
return;
}
_20(_24,_24.parentNode);
};
});
