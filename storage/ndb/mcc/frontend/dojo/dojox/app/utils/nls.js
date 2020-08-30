//>>built
define("dojox/app/utils/nls",["require","dojo/Deferred"],function(_1,_2){
return function(_3,_4){
var _5=_3.nls;
if(_5){
var _6=new _2();
var _7;
try{
var _8=_5;
var _9=_8.indexOf("./");
if(_9>=0){
_8=_5.substring(_9+2);
}
_7=_1.on?_1.on("error",function(_a){
if(_6.isResolved()||_6.isRejected()){
return;
}
if(_a.info[0]&&(_a.info[0].indexOf(_8)>=0)){
_6.resolve(false);
if(_7){
_7.remove();
}
}
}):null;
if(_5.indexOf("./")==0){
_5="app/"+_5;
}
_1(["dojo/i18n!"+_5],function(_b){
_6.resolve(_b);
_7.remove();
});
}
catch(e){
_6.reject(e);
if(_7){
_7.remove();
}
}
return _6;
}else{
return false;
}
};
});
