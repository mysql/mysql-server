/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom",["./_base/sniff","./_base/lang","./_base/window"],function(_1,_2,_3){
try{
document.execCommand("BackgroundImageCache",false,true);
}
catch(e){
}
var _4={};
if(_1("ie")){
_4.byId=function(id,_5){
if(typeof id!="string"){
return id;
}
var _6=_5||_3.doc,te=id&&_6.getElementById(id);
if(te&&(te.attributes.id.value==id||te.id==id)){
return te;
}else{
var _7=_6.all[id];
if(!_7||_7.nodeName){
_7=[_7];
}
var i=0;
while((te=_7[i++])){
if((te.attributes&&te.attributes.id&&te.attributes.id.value==id)||te.id==id){
return te;
}
}
}
};
}else{
_4.byId=function(id,_8){
return ((typeof id=="string")?(_8||_3.doc).getElementById(id):id)||null;
};
}
_4.isDescendant=function(_9,_a){
try{
_9=_4.byId(_9);
_a=_4.byId(_a);
while(_9){
if(_9==_a){
return true;
}
_9=_9.parentNode;
}
}
catch(e){
}
return false;
};
_4.setSelectable=function(_b,_c){
_b=_4.byId(_b);
if(_1("mozilla")){
_b.style.MozUserSelect=_c?"":"none";
}else{
if(_1("khtml")||_1("webkit")){
_b.style.KhtmlUserSelect=_c?"auto":"none";
}else{
if(_1("ie")){
var v=(_b.unselectable=_c?"":"on"),cs=_b.getElementsByTagName("*"),i=0,l=cs.length;
for(;i<l;++i){
cs.item(i).unselectable=v;
}
}
}
}
};
return _4;
});
