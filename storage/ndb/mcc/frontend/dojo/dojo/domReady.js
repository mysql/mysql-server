/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/domReady",["./has"],function(_1){
var _2=this,_3=document,_4={"loaded":1,"complete":1},_5=typeof _3.readyState!="string",_6=!!_4[_3.readyState];
if(_5){
_3.readyState="loading";
}
if(!_6){
var _7=[],_8=[],_9=function(_a){
_a=_a||_2.event;
if(_6||(_a.type=="readystatechange"&&!_4[_3.readyState])){
return;
}
_6=1;
if(_5){
_3.readyState="complete";
}
while(_7.length){
(_7.shift())();
}
},on=function(_b,_c){
_b.addEventListener(_c,_9,false);
_7.push(function(){
_b.removeEventListener(_c,_9,false);
});
};
if(!_1("dom-addeventlistener")){
on=function(_d,_e){
_e="on"+_e;
_d.attachEvent(_e,_9);
_7.push(function(){
_d.detachEvent(_e,_9);
});
};
var _f=_3.createElement("div");
try{
if(_f.doScroll&&_2.frameElement===null){
_8.push(function(){
try{
_f.doScroll("left");
return 1;
}
catch(e){
}
});
}
}
catch(e){
}
}
on(_3,"DOMContentLoaded");
on(_2,"load");
if("onreadystatechange" in _3){
on(_3,"readystatechange");
}else{
if(!_5){
_8.push(function(){
return _4[_3.readyState];
});
}
}
if(_8.length){
var _10=function(){
if(_6){
return;
}
var i=_8.length;
while(i--){
if(_8[i]()){
_9("poller");
return;
}
}
setTimeout(_10,30);
};
_10();
}
}
function _11(_12){
if(_6){
_12(1);
}else{
_7.push(_12);
}
};
_11.load=function(id,req,_13){
_11(_13);
};
return _11;
});
