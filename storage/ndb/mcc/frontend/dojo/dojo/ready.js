/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/ready",["./_base/kernel","./has","require","./domReady","./_base/lang"],function(_1,_2,_3,_4,_5){
var _6=0,_7,_8=[],_9=0,_a=function(){
_6=1;
_1._postLoad=_1.config.afterOnLoad=true;
if(_8.length){
_7(_b);
}
},_b=function(){
if(_6&&!_9&&_8.length){
_9=1;
var f=_8.shift();
try{
f();
}
finally{
_9=0;
}
_9=0;
if(_8.length){
_7(_b);
}
}
};
if(1){
_3.on("idle",_b);
_7=function(){
if(_3.idle()){
_b();
}
};
}else{
_7=function(){
_3.ready(_b);
};
}
var _c=_1.ready=_1.addOnLoad=function(_d,_e,_f){
var _10=_5._toArray(arguments);
if(typeof _d!="number"){
_f=_e;
_e=_d;
_d=1000;
}else{
_10.shift();
}
_f=_f?_5.hitch.apply(_1,_10):function(){
_e();
};
_f.priority=_d;
for(var i=0;i<_8.length&&_d>=_8[i].priority;i++){
}
_8.splice(i,0,_f);
_7();
};
true||_2.add("dojo-config-addOnLoad",1);
if(1){
var dca=_1.config.addOnLoad;
if(dca){
_c[(_5.isArray(dca)?"apply":"call")](_1,dca);
}
}
if(1&&_1.config.parseOnLoad&&!_1.isAsync){
_c(99,function(){
if(!_1.parser){
_1.deprecated("Add explicit require(['dojo/parser']);","","2.0");
_3(["dojo/parser"]);
}
});
}
if(1){
_4(_a);
}else{
_a();
}
return _c;
});
