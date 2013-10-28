//>>built
define("dojox/grid/enhanced/_PluginManager",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","./_Events","./_FocusManager","../util"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_3("dojox.grid.enhanced._PluginManager",null,{_options:null,_plugins:null,_connects:null,constructor:function(_a){
this.grid=_a;
this._store=_a.store;
this._options={};
this._plugins=[];
this._connects=[];
this._parseProps(this.grid.plugins);
_a.connect(_a,"_setStore",_2.hitch(this,function(_b){
if(this._store!==_b){
this.forEach("onSetStore",[_b,this._store]);
this._store=_b;
}
}));
},startup:function(){
this.forEach("onStartUp");
},preInit:function(){
this.grid.focus.destroy();
this.grid.focus=new _7(this.grid);
new _6(this.grid);
this._init(true);
this.forEach("onPreInit");
},postInit:function(){
this._init(false);
_4.forEach(this.grid.views.views,this._initView,this);
this._connects.push(_5.connect(this.grid.views,"addView",_2.hitch(this,this._initView)));
if(this._plugins.length>0){
var _c=this.grid.edit;
if(_c){
_c.styleRow=function(_d){
};
}
}
this.forEach("onPostInit");
},forEach:function(_e,_f){
_4.forEach(this._plugins,function(p){
if(!p||!p[_e]){
return;
}
p[_e].apply(p,_f?_f:[]);
});
},_parseProps:function(_10){
if(!_10){
return;
}
var p,_11={},_12=this._options,_13=this.grid;
var _14=_9.registry;
for(p in _10){
if(_10[p]){
this._normalize(p,_10,_14,_11);
}
}
if(_12.dnd||_12.indirectSelection){
_12.columnReordering=false;
}
_2.mixin(_13,_12);
},_normalize:function(p,_15,_16,_17){
if(!_16[p]){
throw new Error("Plugin "+p+" is required.");
}
if(_17[p]){
throw new Error("Recursive cycle dependency is not supported.");
}
var _18=this._options;
if(_18[p]){
return _18[p];
}
_17[p]=true;
_18[p]=_2.mixin({},_16[p],_2.isObject(_15[p])?_15[p]:{});
var _19=_18[p]["dependency"];
if(_19){
if(!_2.isArray(_19)){
_19=_18[p]["dependency"]=[_19];
}
_4.forEach(_19,function(_1a){
if(!this._normalize(_1a,_15,_16,_17)){
throw new Error("Plugin "+_1a+" is required.");
}
},this);
}
delete _17[p];
return _18[p];
},_init:function(pre){
var p,_1b,_1c=this._options;
for(p in _1c){
_1b=_1c[p]["preInit"];
if((pre?_1b:!_1b)&&_1c[p]["class"]&&!this.pluginExisted(p)){
this.loadPlugin(p);
}
}
},loadPlugin:function(_1d){
var _1e=this._options[_1d];
if(!_1e){
return null;
}
var _1f=this.getPlugin(_1d);
if(_1f){
return _1f;
}
var _20=_1e["dependency"];
_4.forEach(_20,function(_21){
if(!this.loadPlugin(_21)){
throw new Error("Plugin "+_21+" is required.");
}
},this);
var cls=_1e["class"];
delete _1e["class"];
_1f=new this.getPluginClazz(cls)(this.grid,_1e);
this._plugins.push(_1f);
return _1f;
},_initView:function(_22){
if(!_22){
return;
}
_8.funnelEvents(_22.contentNode,_22,"doContentEvent",["mouseup","mousemove"]);
_8.funnelEvents(_22.headerNode,_22,"doHeaderEvent",["mouseup"]);
},pluginExisted:function(_23){
return !!this.getPlugin(_23);
},getPlugin:function(_24){
var _25=this._plugins;
_24=_24.toLowerCase();
for(var i=0,len=_25.length;i<len;i++){
if(_24==_25[i]["name"].toLowerCase()){
return _25[i];
}
}
return null;
},getPluginClazz:function(_26){
if(_2.isFunction(_26)){
return _26;
}
var _27="Please make sure Plugin \""+_26+"\" is existed.";
try{
var cls=_2.getObject(_26);
if(!cls){
throw new Error(_27);
}
return cls;
}
catch(e){
throw new Error(_27);
}
},isFixedCell:function(_28){
return _28&&(_28.isRowSelector||_28.fixedPos);
},destroy:function(){
_4.forEach(this._connects,_5.disconnect);
this.forEach("destroy");
if(this.grid.unwrap){
this.grid.unwrap();
}
delete this._connects;
delete this._plugins;
delete this._options;
}});
_9.registerPlugin=function(_29,_2a){
if(!_29){
console.warn("Failed to register plugin, class missed!");
return;
}
var cls=_9;
cls.registry=cls.registry||{};
cls.registry[_29.prototype.name]=_2.mixin({"class":_29},(_2a?_2a:{}));
};
return _9;
});
