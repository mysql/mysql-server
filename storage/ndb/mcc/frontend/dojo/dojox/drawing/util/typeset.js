//>>built
define("dojox/drawing/util/typeset",["../library/greek"],function(_1){
return {convertHTML:function(_2){
if(_2){
return _2.replace(/&([^;]+);/g,function(_3,_4){
if(_4.charAt(0)=="#"){
var _5=+_4.substr(1);
if(!isNaN(_5)){
return String.fromCharCode(_5);
}
}else{
if(_1[_4]){
return String.fromCharCode(_1[_4]);
}
}
console.warn("no HTML conversion for ",_3);
return _3;
});
}
return _2;
},convertLaTeX:function(_6){
if(_6){
return _6.replace(/\\([a-zA-Z]+)/g,function(_7,_8){
if(_1[_8]){
return String.fromCharCode(_1[_8]);
}else{
if(_8.substr(0,2)=="mu"){
return String.fromCharCode(_1["mu"])+_8.substr(2);
}else{
if(_8.substr(0,5)=="theta"){
return String.fromCharCode(_1["theta"])+_8.substr(5);
}else{
if(_8.substr(0,3)=="phi"){
return String.fromCharCode(_1["phi"])+_8.substr(3);
}
}
}
}
}).replace(/\\\\/g,"\\");
}
return _6;
}};
});
