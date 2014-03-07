/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/string",["./_base/kernel","./_base/lang"],function(_1,_2){
_2.getObject("string",true,_1);
_1.string.rep=function(_3,_4){
if(_4<=0||!_3){
return "";
}
var _5=[];
for(;;){
if(_4&1){
_5.push(_3);
}
if(!(_4>>=1)){
break;
}
_3+=_3;
}
return _5.join("");
};
_1.string.pad=function(_6,_7,ch,_8){
if(!ch){
ch="0";
}
var _9=String(_6),_a=_1.string.rep(ch,Math.ceil((_7-_9.length)/ch.length));
return _8?_9+_a:_a+_9;
};
_1.string.substitute=function(_b,_c,_d,_e){
_e=_e||_1.global;
_d=_d?_2.hitch(_e,_d):function(v){
return v;
};
return _b.replace(/\$\{([^\s\:\}]+)(?:\:([^\s\:\}]+))?\}/g,function(_f,key,_10){
var _11=_2.getObject(key,false,_c);
if(_10){
_11=_2.getObject(_10,false,_e).call(_e,_11,key);
}
return _d(_11,key).toString();
});
};
_1.string.trim=String.prototype.trim?_2.trim:function(str){
str=str.replace(/^\s+/,"");
for(var i=str.length-1;i>=0;i--){
if(/\S/.test(str.charAt(i))){
str=str.substring(0,i+1);
break;
}
}
return str;
};
return _1.string;
});
