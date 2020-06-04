/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_firebug/firebug",[],function(){
var _1=(/Trident/.test(window.navigator.userAgent));
if(_1){
var _2=["log","info","debug","warn","error"];
for(var i=0;i<_2.length;i++){
var m=_2[i];
if(!console[m]||console[m]._fake){
continue;
}
var n="_"+_2[i];
console[n]=console[m];
console[m]=(function(){
var _3=n;
return function(){
console[_3](Array.prototype.join.call(arguments," "));
};
})();
}
try{
console.clear();
}
catch(e){
}
}
});
