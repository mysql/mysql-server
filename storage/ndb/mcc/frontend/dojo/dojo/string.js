/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/string",["./_base/kernel","./_base/lang"],function(_1,_2){
var _3=/[&<>'"\/]/g;
var _4={"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;","'":"&#x27;","/":"&#x2F;"};
var _5={};
_2.setObject("dojo.string",_5);
_5.escape=function(_6){
if(!_6){
return "";
}
return _6.replace(_3,function(c){
return _4[c];
});
};
_5.rep=function(_7,_8){
if(_8<=0||!_7){
return "";
}
var _9=[];
for(;;){
if(_8&1){
_9.push(_7);
}
if(!(_8>>=1)){
break;
}
_7+=_7;
}
return _9.join("");
};
_5.pad=function(_a,_b,ch,_c){
if(!ch){
ch="0";
}
var _d=String(_a),_e=_5.rep(ch,Math.ceil((_b-_d.length)/ch.length));
return _c?_d+_e:_e+_d;
};
_5.substitute=function(_f,map,_10,_11){
_11=_11||_1.global;
_10=_10?_2.hitch(_11,_10):function(v){
return v;
};
return _f.replace(/\$\{([^\s\:\}]*)(?:\:([^\s\:\}]+))?\}/g,function(_12,key,_13){
if(key==""){
return "$";
}
var _14=_2.getObject(key,false,map);
if(_13){
_14=_2.getObject(_13,false,_11).call(_11,_14,key);
}
var _15=_10(_14,key);
if(typeof _15==="undefined"){
throw new Error("string.substitute could not find key \""+key+"\" in template");
}
return _15.toString();
});
};
_5.trim=String.prototype.trim?_2.trim:function(str){
str=str.replace(/^\s+/,"");
for(var i=str.length-1;i>=0;i--){
if(/\S/.test(str.charAt(i))){
str=str.substring(0,i+1);
break;
}
}
return str;
};
return _5;
});
