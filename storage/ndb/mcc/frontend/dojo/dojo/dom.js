/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom",["./sniff","./_base/window","./_base/kernel"],function(_1,_2,_3){
if(_1("ie")<=7){
try{
document.execCommand("BackgroundImageCache",false,true);
}
catch(e){
}
}
var _4={};
if(_1("ie")){
_4.byId=function(id,_5){
if(typeof id!="string"){
return id||null;
}
var _6=_5||_2.doc,te=id&&_6.getElementById(id);
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
return null;
};
}else{
_4.byId=function(id,_8){
return ((typeof id=="string")?(_8||_2.doc).getElementById(id):id)||null;
};
}
var _9=_3.global["document"]||null;
_1.add("dom-contains",!!(_9&&_9.contains));
_4.isDescendant=_1("dom-contains")?function(_a,_b){
return !!((_b=_4.byId(_b))&&_b.contains(_4.byId(_a)));
}:function(_c,_d){
try{
_c=_4.byId(_c);
_d=_4.byId(_d);
while(_c){
if(_c==_d){
return true;
}
_c=_c.parentNode;
}
}
catch(e){
}
return false;
};
_1.add("css-user-select",function(_e,_f,_10){
if(!_10){
return false;
}
var _11=_10.style;
var _12=["Khtml","O","Moz","Webkit"],i=_12.length,_13="userSelect",_14;
do{
if(typeof _11[_13]!=="undefined"){
return _13;
}
}while(i--&&(_13=_12[i]+"UserSelect"));
return false;
});
var _15=_1("css-user-select");
_4.setSelectable=_15?function(_16,_17){
_4.byId(_16).style[_15]=_17?"":"none";
}:function(_18,_19){
_18=_4.byId(_18);
var _1a=_18.getElementsByTagName("*"),i=_1a.length;
if(_19){
_18.removeAttribute("unselectable");
while(i--){
_1a[i].removeAttribute("unselectable");
}
}else{
_18.setAttribute("unselectable","on");
while(i--){
_1a[i].setAttribute("unselectable","on");
}
}
};
return _4;
});
