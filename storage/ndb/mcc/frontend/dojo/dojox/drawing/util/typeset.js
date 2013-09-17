//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/library/greek"],function(_1,_2,_3){
_2.provide("dojox.drawing.util.typeset");
_2.require("dojox.drawing.library.greek");
(function(){
var _4=_3.drawing.library.greek;
_3.drawing.util.typeset={convertHTML:function(_5){
if(_5){
return _5.replace(/&([^;]+);/g,function(_6,_7){
if(_7.charAt(0)=="#"){
var _8=+_7.substr(1);
if(!isNaN(_8)){
return String.fromCharCode(_8);
}
}else{
if(_4[_7]){
return String.fromCharCode(_4[_7]);
}
}
console.warn("no HTML conversion for ",_6);
return _6;
});
}
return _5;
},convertLaTeX:function(_9){
if(_9){
return _9.replace(/\\([a-zA-Z]+)/g,function(_a,_b){
if(_4[_b]){
return String.fromCharCode(_4[_b]);
}else{
if(_b.substr(0,2)=="mu"){
return String.fromCharCode(_4["mu"])+_b.substr(2);
}else{
if(_b.substr(0,5)=="theta"){
return String.fromCharCode(_4["theta"])+_b.substr(5);
}else{
if(_b.substr(0,3)=="phi"){
return String.fromCharCode(_4["phi"])+_b.substr(3);
}
}
}
}
}).replace(/\\\\/g,"\\");
}
return _9;
}};
})();
});
