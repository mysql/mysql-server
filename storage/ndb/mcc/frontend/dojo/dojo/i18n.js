/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/i18n",["./_base/kernel","require","./has","./_base/array","./_base/lang","./_base/xhr"],function(_1,_2,_3,_4,_5){
var _6=_1.i18n={},_7=/(^.*(^|\/)nls)(\/|$)([^\/]*)\/?([^\/]*)/,_8=function(_9,_a,_b,_c){
for(var _d=[_b+_c],_e=_a.split("-"),_f="",i=0;i<_e.length;i++){
_f+=(_f?"-":"")+_e[i];
if(!_9||_9[_f]){
_d.push(_b+_f+"/"+_c);
}
}
return _d;
},_10={},_11=_1.getL10nName=function(_12,_13,_14){
_14=_14?_14.toLowerCase():_1.locale;
_12="dojo/i18n!"+_12.replace(/\./g,"/");
_13=_13.replace(/\./g,"/");
return (/root/i.test(_14))?(_12+"/nls/"+_13):(_12+"/nls/"+_14+"/"+_13);
},_15=function(_16,_17,_18,_19,_1a,_1b){
_16([_17],function(_1c){
var _1d=_10[_17+"/"]=_5.clone(_1c.root),_1e=_8(!_1c._v1x&&_1c,_1a,_18,_19);
_16(_1e,function(){
for(var i=1;i<_1e.length;i++){
_10[_1e[i]]=_1d=_5.mixin(_5.clone(_1d),arguments[i]);
}
var _1f=_17+"/"+_1a;
_10[_1f]=_1d;
_1b&&_1b(_5.delegate(_1d));
});
});
},_20=function(id,_21){
var _22=_7.exec(id),_23=_22[1];
return /^\./.test(_23)?_21(_23)+"/"+id.substring(_23.length):id;
};
load=function(id,_24,_25){
var _26=_7.exec(id),_27=_26[1]+"/",_28=_26[5]||_26[4],_29=_27+_28,_2a=(_26[5]&&_26[4]),_2b=_2a||_1.locale,_2c=_29+"/"+_2b;
if(_2a){
if(_10[_2c]){
_25(_10[_2c]);
}else{
_15(_24,_29,_27,_28,_2b,_25);
}
return;
}
var _2d=_1.config.extraLocale||[];
_2d=_5.isArray(_2d)?_2d:[_2d];
_2d.push(_2b);
_4.forEach(_2d,function(_2e){
_15(_24,_29,_27,_28,_2e,_2e==_2b&&_25);
});
};
true||_3.add("dojo-v1x-i18n-Api",1);
if(1){
var _2f=new Function("bundle","var __preAmdResult, __amdResult; function define(bundle){__amdResult= bundle;} __preAmdResult= eval(bundle); return [__preAmdResult, __amdResult];"),_30=function(url,_31,_32){
return _31?(/nls\/[^\/]+\/[^\/]+$/.test(url)?_31:{root:_31,_v1x:1}):_32;
},_33=function(_34,_35){
var _36=[];
_1.forEach(_34,function(mid){
var url=_2.toUrl(mid+".js");
if(_10[url]){
_36.push(_10[url]);
}else{
try{
var _37=_2(mid);
if(_37){
_36.push(_37);
return;
}
}
catch(e){
}
_1.xhrGet({url:url,sync:true,load:function(_38){
var _39=_2f(_38);
_36.push(_10[url]=_30(url,_39[0],_39[1]));
},error:function(){
_36.push(_10[url]={});
}});
}
});
_35.apply(null,_36);
};
_6.getLocalization=function(_3a,_3b,_3c){
var _3d,_3e=_11(_3a,_3b,_3c).substring(10);
load(_3e,(1&&!_2.isXdUrl(_2.toUrl(_3e+".js"))?_33:_2),function(_3f){
_3d=_3f;
});
return _3d;
};
_6.normalizeLocale=function(_40){
var _41=_40?_40.toLowerCase():_1.locale;
if(_41=="root"){
_41="ROOT";
}
return _41;
};
}
return _5.mixin(_6,{dynamic:true,normalize:_20,load:load,cache:function(mid,_42){
_10[mid]=_42;
}});
});
