/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/i18n",["./_base/kernel","require","./has","./_base/array","./_base/config","./_base/lang","./_base/xhr","./json","module"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
_3.add("dojo-preload-i18n-Api",1);
1||_3.add("dojo-v1x-i18n-Api",1);
var _a=_1.i18n={},_b=/(^.*(^|\/)nls)(\/|$)([^\/]*)\/?([^\/]*)/,_c=function(_d,_e,_f,_10){
for(var _11=[_f+_10],_12=_e.split("-"),_13="",i=0;i<_12.length;i++){
_13+=(_13?"-":"")+_12[i];
if(!_d||_d[_13]){
_11.push(_f+_13+"/"+_10);
_11.specificity=_13;
}
}
return _11;
},_14={},_15=function(_16,_17,_18){
_18=_18?_18.toLowerCase():_1.locale;
_16=_16.replace(/\./g,"/");
_17=_17.replace(/\./g,"/");
return (/root/i.test(_18))?(_16+"/nls/"+_17):(_16+"/nls/"+_18+"/"+_17);
},_19=_1.getL10nName=function(_1a,_1b,_1c){
return _1a=_9.id+"!"+_15(_1a,_1b,_1c);
},_1d=function(_1e,_1f,_20,_21,_22,_23){
_1e([_1f],function(_24){
var _25=_6.clone(_24.root||_24.ROOT),_26=_c(!_24._v1x&&_24,_22,_20,_21);
_1e(_26,function(){
for(var i=1;i<_26.length;i++){
_25=_6.mixin(_6.clone(_25),arguments[i]);
}
var _27=_1f+"/"+_22;
_14[_27]=_25;
_25.$locale=_26.specificity;
_23();
});
});
},_28=function(id,_29){
return /^\./.test(id)?_29(id):id;
},_2a=function(_2b){
var _2c=_5.extraLocale||[];
_2c=_6.isArray(_2c)?_2c:[_2c];
_2c.push(_2b);
return _2c;
},_2d=function(id,_2e,_2f){
var _30=_b.exec(id),_31=_30[1]+"/",_32=_30[5]||_30[4],_33=_31+_32,_34=(_30[5]&&_30[4]),_35=_34||_1.locale||"",_36=_33+"/"+_35,_37=_34?[_35]:_2a(_35),_38=_37.length,_39=function(){
if(!--_38){
_2f(_6.delegate(_14[_36]));
}
},_3a=id.split("*"),_3b=_3a[1]=="preload";
if(_3("dojo-preload-i18n-Api")){
if(_3b){
if(!_14[id]){
_14[id]=1;
_46(_3a[2],_8.parse(_3a[3]),1,_2e);
}
_2f(1);
}
if(_3b||(_67(id,_2e,_2f)&&!_14[_36])){
return;
}
}else{
if(_3b){
_2f(1);
return;
}
}
_4.forEach(_37,function(_3c){
var _3d=_33+"/"+_3c;
if(_3("dojo-preload-i18n-Api")){
_3e(_3d);
}
if(!_14[_3d]){
_1d(_2e,_33,_31,_32,_3c,_39);
}else{
_39();
}
});
};
if(_3("dojo-preload-i18n-Api")||1){
var _3f=_a.normalizeLocale=function(_40){
var _41=_40?_40.toLowerCase():_1.locale;
return _41=="root"?"ROOT":_41;
},_42=function(mid,_43){
return (1&&1)?_43.isXdUrl(_2.toUrl(mid+".js")):true;
},_44=0,_45=[],_46=_a._preloadLocalizations=function(_47,_48,_49,_4a){
_4a=_4a||_2;
function _4b(mid,_4c){
if(_42(mid,_4a)||_49){
_4a([mid],_4c);
}else{
_6d([mid],_4c,_4a);
}
};
function _4d(_4e,_4f){
var _50=_4e.split("-");
while(_50.length){
if(_4f(_50.join("-"))){
return;
}
_50.pop();
}
_4f("ROOT");
};
function _51(){
_44++;
};
function _52(){
--_44;
while(!_44&&_45.length){
_2d.apply(null,_45.shift());
}
};
function _53(_54,_55,loc,_56){
return _56.toAbsMid(_54+_55+"/"+loc);
};
function _57(_58){
_58=_3f(_58);
_4d(_58,function(loc){
if(_4.indexOf(_48,loc)>=0){
var mid=_47.replace(/\./g,"/")+"_"+loc;
_51();
_4b(mid,function(_59){
for(var p in _59){
var _5a=_59[p],_5b=p.match(/(.+)\/([^\/]+)$/),_5c,_5d;
if(!_5b){
continue;
}
_5c=_5b[2];
_5d=_5b[1]+"/";
if(!_5a._localized){
continue;
}
var _5e;
if(loc==="ROOT"){
var _5f=_5e=_5a._localized;
delete _5a._localized;
_5f.root=_5a;
_14[_2.toAbsMid(p)]=_5f;
}else{
_5e=_5a._localized;
_14[_53(_5d,_5c,loc,_2)]=_5a;
}
if(loc!==_58){
function _60(_61,_62,_63,_64){
var _65=[],_66=[];
_4d(_58,function(loc){
if(_64[loc]){
_65.push(_2.toAbsMid(_61+loc+"/"+_62));
_66.push(_53(_61,_62,loc,_2));
}
});
if(_65.length){
_51();
_4a(_65,function(){
for(var i=_65.length-1;i>=0;i--){
_63=_6.mixin(_6.clone(_63),arguments[i]);
_14[_66[i]]=_63;
}
_14[_53(_61,_62,_58,_2)]=_6.clone(_63);
_52();
});
}else{
_14[_53(_61,_62,_58,_2)]=_63;
}
};
_60(_5d,_5c,_5a,_5e);
}
}
_52();
});
return true;
}
return false;
});
};
_57();
_4.forEach(_1.config.extraLocale,_57);
},_67=function(id,_68,_69){
if(_44){
_45.push([id,_68,_69]);
}
return _44;
},_3e=function(){
};
}
if(1){
var _6a={},_6b={},_6c,_6d=function(_6e,_6f,_70){
var _71=[];
_4.forEach(_6e,function(mid){
var url=_70.toUrl(mid+".js");
function _2d(_72){
if(!_6c){
_6c=new Function("__bundle","__checkForLegacyModules","__mid","__amdValue","var define = function(mid, factory){define.called = 1; __amdValue.result = factory || mid;},"+"\t   require = function(){define.called = 1;};"+"try{"+"define.called = 0;"+"eval(__bundle);"+"if(define.called==1)"+"return __amdValue;"+"if((__checkForLegacyModules = __checkForLegacyModules(__mid)))"+"return __checkForLegacyModules;"+"}catch(e){}"+"try{"+"return eval('('+__bundle+')');"+"}catch(e){"+"return e;"+"}");
}
var _73=_6c(_72,_3e,mid,_6a);
if(_73===_6a){
_71.push(_14[url]=_6a.result);
}else{
if(_73 instanceof Error){
console.error("failed to evaluate i18n bundle; url="+url,_73);
_73={};
}
_71.push(_14[url]=(/nls\/[^\/]+\/[^\/]+$/.test(url)?_73:{root:_73,_v1x:1}));
}
};
if(_14[url]){
_71.push(_14[url]);
}else{
var _74=_70.syncLoadNls(mid);
if(!_74){
_74=_3e(mid.replace(/nls\/([^\/]*)\/([^\/]*)$/,"nls/$2/$1"));
}
if(_74){
_71.push(_74);
}else{
if(!_7){
try{
_70.getText(url,true,_2d);
}
catch(e){
_71.push(_14[url]={});
}
}else{
_7.get({url:url,sync:true,load:_2d,error:function(){
_71.push(_14[url]={});
}});
}
}
}
});
_6f&&_6f.apply(null,_71);
};
_3e=function(_75){
for(var _76,_77=_75.split("/"),_78=_1.global[_77[0]],i=1;_78&&i<_77.length-1;_78=_78[_77[i++]]){
}
if(_78){
_76=_78[_77[i]];
if(!_76){
_76=_78[_77[i].replace(/-/g,"_")];
}
if(_76){
_14[_75]=_76;
}
}
return _76;
};
_a.getLocalization=function(_79,_7a,_7b){
var _7c,_7d=_15(_79,_7a,_7b);
if(_6b[_7d]){
return _6b[_7d];
}
_2d(_7d,(!_42(_7d,_2)?function(_7e,_7f){
_6d(_7e,_7f,_2);
}:_2),function(_80){
_6b[_7d]=_80;
_7c=_80;
});
return _7c;
};
}else{
_a.getLocalization=function(_81,_82,_83){
var key=_81.replace(/\./g,"/")+"/nls/"+_82+"/"+(_83||_5.locale);
return this.cache[key];
};
}
return _6.mixin(_a,{dynamic:true,normalize:_28,load:_2d,cache:_14,getL10nName:_19});
});
