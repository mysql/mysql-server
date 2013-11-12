//>>built
define("dojox/grid/enhanced/plugins/_StoreLayer",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/xhr"],function(_1,_2,_3,_4){
var ns=_3.getObject("grid.enhanced.plugins",true,dojox);
var _5=function(_6){
var _7=["reorder","sizeChange","normal","presentation"];
var _8=_7.length;
for(var i=_6.length-1;i>=0;--i){
var p=_2.indexOf(_7,_6[i]);
if(p>=0&&p<=_8){
_8=p;
}
}
if(_8<_7.length-1){
return _7.slice(0,_8+1);
}else{
return _7;
}
},_9=function(_a){
var i,_b=this._layers,_c=_b.length;
if(_a){
for(i=_c-1;i>=0;--i){
if(_b[i].name()==_a){
_b[i]._unwrap(_b[i+1]);
break;
}
}
_b.splice(i,1);
}else{
for(i=_c-1;i>=0;--i){
_b[i]._unwrap();
}
}
if(!_b.length){
delete this._layers;
delete this.layer;
delete this.unwrap;
delete this.forEachLayer;
}
return this;
},_d=function(_e){
var i,_f=this._layers;
if(typeof _e=="undefined"){
return _f.length;
}
if(typeof _e=="number"){
return _f[_e];
}
for(i=_f.length-1;i>=0;--i){
if(_f[i].name()==_e){
return _f[i];
}
}
return null;
},_10=function(_11,_12){
var len=this._layers.length,_13,end,dir;
if(_12){
_13=0;
end=len;
dir=1;
}else{
_13=len-1;
end=-1;
dir=-1;
}
for(var i=_13;i!=end;i+=dir){
if(_11(this._layers[i],i)===false){
return i;
}
}
return end;
};
ns.wrap=function(_14,_15,_16,_17){
if(!_14._layers){
_14._layers=[];
_14.layer=_3.hitch(_14,_d);
_14.unwrap=_3.hitch(_14,_9);
_14.forEachLayer=_3.hitch(_14,_10);
}
var _18=_5(_16.tags);
if(!_2.some(_14._layers,function(lyr,i){
if(_2.some(lyr.tags,function(tag){
return _2.indexOf(_18,tag)>=0;
})){
return false;
}else{
_14._layers.splice(i,0,_16);
_16._wrap(_14,_15,_17,lyr);
return true;
}
})){
_14._layers.push(_16);
_16._wrap(_14,_15,_17);
}
return _14;
};
var _19=_1("dojox.grid.enhanced.plugins._StoreLayer",null,{tags:["normal"],layerFuncName:"_fetch",constructor:function(){
this._store=null;
this._originFetch=null;
this.__enabled=true;
},initialize:function(_1a){
},uninitialize:function(_1b){
},invalidate:function(){
},_wrap:function(_1c,_1d,_1e,_1f){
this._store=_1c;
this._funcName=_1d;
var _20=_3.hitch(this,function(){
return (this.enabled()?this[_1e||this.layerFuncName]:this.originFetch).apply(this,arguments);
});
if(_1f){
this._originFetch=_1f._originFetch;
_1f._originFetch=_20;
}else{
this._originFetch=_1c[_1d]||function(){
};
_1c[_1d]=_20;
}
this.initialize(_1c);
},_unwrap:function(_21){
this.uninitialize(this._store);
if(_21){
_21._originFetch=this._originFetch;
}else{
this._store[this._funcName]=this._originFetch;
}
this._originFetch=null;
this._store=null;
},enabled:function(_22){
if(typeof _22!="undefined"){
this.__enabled=!!_22;
}
return this.__enabled;
},name:function(){
if(!this.__name){
var m=this.declaredClass.match(/(?:\.(?:_*)([^\.]+)Layer$)|(?:\.([^\.]+)$)/i);
this.__name=m?(m[1]||m[2]).toLowerCase():this.declaredClass;
}
return this.__name;
},originFetch:function(){
return (_3.hitch(this._store,this._originFetch)).apply(this,arguments);
}});
var _23=_1("dojox.grid.enhanced.plugins._ServerSideLayer",_19,{constructor:function(_24){
_24=_24||{};
this._url=_24.url||"";
this._isStateful=!!_24.isStateful;
this._onUserCommandLoad=_24.onCommandLoad||function(){
};
this.__cmds={cmdlayer:this.name(),enable:true};
this.useCommands(this._isStateful);
},enabled:function(_25){
var res=this.inherited(arguments);
this.__cmds.enable=this.__enabled;
return res;
},useCommands:function(_26){
if(typeof _26!="undefined"){
this.__cmds.cmdlayer=(_26&&this._isStateful)?this.name():null;
}
return !!(this.__cmds.cmdlayer);
},_fetch:function(_27){
if(this.__cmds.cmdlayer){
_4.post({url:this._url||this._store.url,content:this.__cmds,load:_3.hitch(this,function(_28){
this.onCommandLoad(_28,_27);
this.originFetch(_27);
}),error:_3.hitch(this,this.onCommandError)});
}else{
this.onCommandLoad("",_27);
this.originFetch(_27);
}
return _27;
},command:function(_29,_2a){
var _2b=this.__cmds;
if(_2a===null){
delete _2b[_29];
}else{
if(typeof _2a!=="undefined"){
_2b[_29]=_2a;
}
}
return _2b[_29];
},onCommandLoad:function(_2c,_2d){
this._onUserCommandLoad(this.__cmds,_2d,_2c);
},onCommandError:function(_2e){
throw _2e;
}});
return {_StoreLayer:_19,_ServerSideLayer:_23,wrap:ns.wrap};
});
