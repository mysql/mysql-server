/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/loader",["./kernel","../has","require","module","./json","./lang","./array"],function(_1,_2,_3,_4,_5,_6,_7){
if(!1){
console.error("cannot load the Dojo v1.x loader with a foreign loader");
return 0;
}
var _8=function(id){
return {src:_4.id,id:id};
},_9=function(_a){
return _a.replace(/\./g,"/");
},_b=/\/\/>>built/,_c=[],_d=[],_e=function(_f,_10,_11){
_c.push(_11);
_7.forEach(_f.split(","),function(mid){
var _12=_13(mid,_10.module);
_d.push(_12);
_14(_12);
});
_15();
},_15=function(){
_d=_7.filter(_d,function(_16){
return _16.injected!==_48&&!_16.executed;
});
if(!_d.length){
_18.holdIdle();
var _17=_c;
_c=[];
_7.forEach(_17,function(cb){
cb(1);
});
_18.releaseIdle();
}
},_19=function(mid,_1a,_1b){
_1a([mid],function(_1c){
_1a(_1c.names,function(){
for(var _1d="",_1e=[],i=0;i<arguments.length;i++){
_1d+="var "+_1c.names[i]+"= arguments["+i+"]; ";
_1e.push(arguments[i]);
}
eval(_1d);
var _1f=_1a.module,_20=[],_21={},_22=[],p,_23={provide:function(_24){
_24=_9(_24);
var _25=_13(_24,_1f);
if(_25!==_1f){
_4f(_25);
}
},require:function(_26,_27){
_26=_9(_26);
_27&&(_13(_26,_1f).result=_49);
_22.push(_26);
},requireLocalization:function(_28,_29,_2a){
_20.length||(_20=["dojo/i18n"]);
_2a=(_2a||_1.locale).toLowerCase();
_28=_9(_28)+"/nls/"+(/root/i.test(_2a)?"":_2a+"/")+_9(_29);
if(_13(_28,_1f).isXd){
_20.push("dojo/i18n!"+_28);
}
},loadInit:function(f){
f();
}};
try{
for(p in _23){
_21[p]=_1[p];
_1[p]=_23[p];
}
_1c.def.apply(null,_1e);
}
catch(e){
_50("error",[_8("failedDojoLoadInit"),e]);
}
finally{
for(p in _23){
_1[p]=_21[p];
}
}
_22.length&&_20.push("dojo/require!"+_22.join(","));
_c.push(_1b);
_7.forEach(_22,function(mid){
var _2b=_13(mid,_1a.module);
_d.push(_2b);
_14(_2b);
});
_15();
});
});
},_2c=function(_2d,_2e,_2f){
var _30=/\(|\)/g,_31=1,_32;
_30.lastIndex=_2e;
while((_32=_30.exec(_2d))){
if(_32[0]==")"){
_31-=1;
}else{
_31+=1;
}
if(_31==0){
break;
}
}
if(_31!=0){
throw "unmatched paren around character "+_30.lastIndex+" in: "+_2d;
}
return [_1.trim(_2d.substring(_2f,_30.lastIndex))+";\n",_30.lastIndex];
},_33=/(\/\*([\s\S]*?)\*\/|\/\/(.*)$)/mg,_34=/(^|\s)dojo\.(loadInit|require|provide|requireLocalization|requireIf|requireAfterIf|platformRequire)\s*\(/mg,_35=/(^|\s)(require|define)\s*\(/m,_36=function(_37,_38){
var _39,_3a,_3b,_3c,_3d=[],_3e=[],_3f=[];
_38=_38||_37.replace(_33,function(_40){
_34.lastIndex=_35.lastIndex=0;
return (_34.test(_40)||_35.test(_40))?"":_40;
});
while((_39=_34.exec(_38))){
_3a=_34.lastIndex;
_3b=_3a-_39[0].length;
_3c=_2c(_38,_3a,_3b);
if(_39[2]=="loadInit"){
_3d.push(_3c[0]);
}else{
_3e.push(_3c[0]);
}
_34.lastIndex=_3c[1];
}
_3f=_3d.concat(_3e);
if(_3f.length||!_35.test(_38)){
return [_37.replace(/(^|\s)dojo\.loadInit\s*\(/g,"\n0 && dojo.loadInit("),_3f.join(""),_3f];
}else{
return 0;
}
},_41=function(_42,_43){
var _44,id,_45=[],_46=[];
if(_b.test(_43)||!(_44=_36(_43))){
return 0;
}
id=_42.mid+"-*loadInit";
for(var p in _13("dojo",_42).result.scopeMap){
_45.push(p);
_46.push("\""+p+"\"");
}
return "// xdomain rewrite of "+_42.path+"\n"+"define('"+id+"',{\n"+"\tnames:"+_1.toJson(_45)+",\n"+"\tdef:function("+_45.join(",")+"){"+_44[1]+"}"+"});\n\n"+"define("+_1.toJson(_45.concat(["dojo/loadInit!"+id]))+", function("+_45.join(",")+"){\n"+_44[0]+"});";
},_18=_3.initSyncLoader(_e,_15,_41),_47=_18.sync,xd=_18.xd,_48=_18.arrived,_49=_18.nonmodule,_4a=_18.executing,_4b=_18.executed,_4c=_18.syncExecStack,_4d=_18.modules,_4e=_18.execQ,_13=_18.getModule,_14=_18.injectModule,_4f=_18.setArrived,_50=_18.signal,_51=_18.finishExec,_52=_18.execModule,_53=_18.getLegacyMode;
_1.provide=function(mid){
var _54=_4c[0],_55=_6.mixin(_13(_9(mid),_3.module),{executed:_4a,result:_6.getObject(mid,true)});
_4f(_55);
if(_54){
(_54.provides||(_54.provides=[])).push(function(){
_55.result=_6.getObject(mid);
delete _55.provides;
_55.executed!==_4b&&_51(_55);
});
}
return _55.result;
};
_2.add("config-publishRequireResult",1,0,0);
_1.require=function(_56,_57){
function _58(mid,_59){
var _5a=_13(_9(mid),_3.module);
if(_4c.length&&_4c[0].finish){
_4c[0].finish.push(mid);
return undefined;
}
if(_5a.executed){
return _5a.result;
}
_59&&(_5a.result=_49);
var _5b=_53();
_14(_5a);
_5b=_53();
if(_5a.executed!==_4b&&_5a.injected===_48){
_18.holdIdle();
_52(_5a);
_18.releaseIdle();
}
if(_5a.executed){
return _5a.result;
}
if(_5b==_47){
if(_5a.cjs){
_4e.unshift(_5a);
}else{
_4c.length&&(_4c[0].finish=[mid]);
}
}else{
_4e.push(_5a);
}
return undefined;
};
var _5c=_58(_56,_57);
if(_2("config-publishRequireResult")&&!_6.exists(_56)&&_5c!==undefined){
_6.setObject(_56,_5c);
}
return _5c;
};
_1.loadInit=function(f){
f();
};
_1.registerModulePath=function(_5d,_5e){
var _5f={};
_5f[_5d.replace(/\./g,"/")]=_5e;
_3({paths:_5f});
};
_1.platformRequire=function(_60){
var _61=(_60.common||[]).concat(_60[_1._name]||_60["default"]||[]),_62;
while(_61.length){
if(_6.isArray(_62=_61.shift())){
_1.require.apply(_1,_62);
}else{
_1.require(_62);
}
}
};
_1.requireIf=_1.requireAfterIf=function(_63,_64,_65){
if(_63){
_1.require(_64,_65);
}
};
_1.requireLocalization=function(_66,_67,_68){
_3(["../i18n"],function(_69){
_69.getLocalization(_66,_67,_68);
});
};
return {extractLegacyApiApplications:_36,require:_18.dojoRequirePlugin,loadInit:_19};
});
