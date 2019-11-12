/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/router/RouterBase",["dojo/_base/declare","dojo/hash","dojo/topic"],function(_1,_2,_3){
var _4;
if(String.prototype.trim){
_4=function(_5){
return _5.trim();
};
}else{
_4=function(_6){
return _6.replace(/^\s\s*/,"").replace(/\s\s*$/,"");
};
}
function _7(_8,_9,_a){
var _b,_c,_d,_e,i,l;
_b=this.callbackQueue;
_c=false;
_d=false;
_e={stopImmediatePropagation:function(){
_c=true;
},preventDefault:function(){
_d=true;
},oldPath:_9,newPath:_a,params:_8};
for(i=0,l=_b.length;i<l;++i){
if(!_c){
_b[i](_e);
}
}
return !_d;
};
var _f=_1(null,{_routes:null,_routeIndex:null,_started:false,_currentPath:"",idMatch:/:(\w[\w\d]*)/g,idReplacement:"([^\\/]+)",globMatch:/\*(\w[\w\d]*)/,globReplacement:"(.+)",constructor:function(_10){
this._routes=[];
this._routeIndex={};
for(var i in _10){
if(_10.hasOwnProperty(i)){
this[i]=_10[i];
}
}
},register:function(_11,_12){
return this._registerRoute(_11,_12);
},registerBefore:function(_13,_14){
return this._registerRoute(_13,_14,true);
},go:function(_15,_16){
var _17;
_15=_4(_15);
_17=this._handlePathChange(_15);
if(_17){
_2(_15,_16);
}
return _17;
},startup:function(){
if(this._started){
return;
}
var _18=this;
this._started=true;
this._handlePathChange(_2());
_3.subscribe("/dojo/hashchange",function(){
_18._handlePathChange.apply(_18,arguments);
});
},_handlePathChange:function(_19){
var i,j,li,lj,_1a,_1b,_1c,_1d,_1e,_1f=this._routes,_20=this._currentPath;
if(!this._started||_19===_20){
return _1c;
}
_1c=true;
for(i=0,li=_1f.length;i<li;++i){
_1a=_1f[i];
_1b=_1a.route.exec(_19);
if(_1b){
if(_1a.parameterNames){
_1d=_1a.parameterNames;
_1e={};
for(j=0,lj=_1d.length;j<lj;++j){
_1e[_1d[j]]=_1b[j+1];
}
}else{
_1e=_1b.slice(1);
}
_1c=_1a.fire(_1e,_20,_19);
}
}
if(_1c){
this._currentPath=_19;
}
return _1c;
},_convertRouteToRegExp:function(_21){
_21=_21.replace(this.idMatch,this.idReplacement);
_21=_21.replace(this.globMatch,this.globReplacement);
_21="^"+_21+"$";
return new RegExp(_21);
},_getParameterNames:function(_22){
var _23=this.idMatch,_24=this.globMatch,_25=[],_26;
_23.lastIndex=0;
while((_26=_23.exec(_22))!==null){
_25.push(_26[1]);
}
if((_26=_24.exec(_22))!==null){
_25.push(_26[1]);
}
return _25.length>0?_25:null;
},_indexRoutes:function(){
var i,l,_27,_28,_29=this._routes;
_28=this._routeIndex={};
for(i=0,l=_29.length;i<l;++i){
_27=_29[i];
_28[_27.route]=i;
}
},_registerRoute:function(_2a,_2b,_2c){
var _2d,_2e,_2f,_30,_31,_32=this,_33=this._routes,_34=this._routeIndex;
_2d=this._routeIndex[_2a];
_2e=typeof _2d!=="undefined";
if(_2e){
_2f=_33[_2d];
}
if(!_2f){
_2f={route:_2a,callbackQueue:[],fire:_7};
}
_30=_2f.callbackQueue;
if(typeof _2a=="string"){
_2f.parameterNames=this._getParameterNames(_2a);
_2f.route=this._convertRouteToRegExp(_2a);
}
if(_2c){
_30.unshift(_2b);
}else{
_30.push(_2b);
}
if(!_2e){
_2d=_33.length;
_34[_2a]=_2d;
_33.push(_2f);
}
_31=false;
return {remove:function(){
var i,l;
if(_31){
return;
}
for(i=0,l=_30.length;i<l;++i){
if(_30[i]===_2b){
_30.splice(i,1);
}
}
if(_30.length===0){
_33.splice(_2d,1);
_32._indexRoutes();
}
_31=true;
},register:function(_35,_36){
return _32.register(_2a,_35,_36);
}};
}});
return _f;
});
