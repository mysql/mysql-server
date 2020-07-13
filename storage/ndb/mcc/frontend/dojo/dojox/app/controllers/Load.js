//>>built
define("dojox/app/controllers/Load",["require","dojo/_base/lang","dojo/_base/declare","dojo/on","dojo/Deferred","dojo/when","dojo/dom-style","../Controller"],function(_1,_2,_3,on,_4,_5,_6,_7,_8){
return _3("dojox.app.controllers.Load",_7,{_waitingQueue:[],constructor:function(_9,_a){
this.events={"app-init":this.init,"app-load":this.load};
},init:function(_b){
_5(this.createView(_b.parent,null,null,{templateString:_b.templateString,controller:_b.controller},null,_b.type),function(_c){
_5(_c.start(),_b.callback);
});
},load:function(_d){
this.app.log("in app/controllers/Load event.viewId="+_d.viewId+" event =",_d);
var _e=_d.viewId||"";
var _f=[];
var _10=_e.split("+");
while(_10.length>0){
var _11=_10.shift();
_f.push(_11);
}
var def;
this.proceedLoadViewDef=new _4();
if(_f&&_f.length>1){
for(var i=0;i<_f.length-1;i++){
var _12=_2.clone(_d);
_12.callback=null;
_12.viewId=_f[i];
this._waitingQueue.push(_12);
}
this.proceedLoadView(this._waitingQueue.shift());
_5(this.proceedLoadViewDef,_2.hitch(this,function(){
var _13=_2.clone(_d);
_13.viewId=_f[i];
def=this.loadView(_13);
return def;
}));
}else{
def=this.loadView(_d);
return def;
}
},proceedLoadView:function(_14){
var def=this.loadView(_14);
_5(def,_2.hitch(this,function(){
this.app.log("in app/controllers/Load proceedLoadView back from loadView for event",_14);
var _15=this._waitingQueue.shift();
if(_15){
this.app.log("in app/controllers/Load proceedLoadView back from loadView calling this.proceedLoadView(nextEvt) for ",_15);
this.proceedLoadView(_15);
}else{
this._waitingQueue=[];
this.proceedLoadViewDef.resolve();
}
}));
},loadView:function(_16){
var _17=_16.parent||this.app;
var _18=_16.viewId||"";
var _19=_18.split(",");
var _1a=_19.shift();
var _1b=_19.join(",");
var _1c=_16.params||"";
this._handleDefault=false;
this._defaultHasPlus=false;
var def=this.loadChild(_17,_1a,_1b,_1c,_16);
if(_16.callback){
_5(def,_2.hitch(this,function(){
if(this._handleDefault&&!_16.initLoad){
this.app.log("logTransitions:",""," emit app-transition this.childViews=["+this.childViews+"]");
this.app.emit("app-transition",{viewId:this.childViews,defaultView:true,forceTransitionNone:_16.forceTransitionNone,opts:{params:_1c}});
}
_16.callback(this._handleDefault,this._defaultHasPlus);
}));
}
return def;
},createChild:function(_1d,_1e,_1f,_20){
var id=_1d.id+"_"+_1e;
if(!_20&&_1d.views[_1e]&&_1d.views[_1e].defaultParams){
_20=_1d.views[_1e].defaultParams;
}
var _21=_1d.children[id];
if(_21){
if(_20){
_21.params=_20;
}
this.app.log("in app/controllers/Load createChild view is already loaded so return the loaded view with the new parms ",_21);
return _21;
}
var def=new _4();
_5(this.createView(_1d,id,_1e,null,_20,_1d.views[_1e].type),function(_22){
_1d.children[id]=_22;
_5(_22.start(),function(_23){
def.resolve(_23);
});
});
return def;
},createView:function(_24,id,_25,_26,_27,_28){
var def=new _4();
var app=this.app;
_1([_28?_28:"../View"],function(_29){
var _2a=new _29(_2.mixin({"app":app,"id":id,"name":_25,"parent":_24},{"params":_27},_26));
def.resolve(_2a);
});
return def;
},loadChild:function(_2b,_2c,_2d,_2e,_2f){
if(!_2b){
throw Error("No parent for Child '"+_2c+"'.");
}
if(!_2c){
var _30=_2b.defaultView?_2b.defaultView.split(","):"default";
if(_2b.defaultView&&!_2f.initLoad){
var _31=this._getViewNamesFromDefaults(_2b);
this.app.log("logTransitions:","Load:loadChild","setting _handleDefault true for parent.defaultView childViews=["+_31+"]");
this._handleDefault=true;
if(_2b.defaultView.indexOf("+")>=0){
this._defaultHasPlus=true;
}
}else{
_2c=_30.shift();
_2d=_30.join(",");
}
}
var _32=new _4();
var _33;
try{
_33=this.createChild(_2b,_2c,_2d,_2e);
}
catch(ex){
console.warn("logTransitions:","","emit reject load exception for =["+_2c+"]",ex);
_32.reject("load child '"+_2c+"' error.");
return _32.promise;
}
_5(_33,_2.hitch(this,function(_34){
if(!_2d&&_34.defaultView){
var _35=this._getViewNamesFromDefaults(_34);
this.app.log("logTransitions:","Load:loadChild"," setting _handleDefault = true child.defaultView childViews=["+_35+"]");
this._handleDefault=true;
if(_34.defaultView.indexOf("+")>=0){
this._defaultHasPlus=true;
}
this.childViews=_35;
_32.resolve();
}
var _36=_2d.split(",");
_2c=_36.shift();
_2d=_36.join(",");
if(_2c){
var _37=this.loadChild(_34,_2c,_2d,_2e,_2f);
_5(_37,function(){
_32.resolve();
},function(){
_32.reject("load child '"+_2c+"' error.");
});
}else{
_32.resolve();
}
}),function(){
console.warn("loadChildDeferred.REJECT() for ["+_2c+"] subIds=["+_2d+"]");
_32.reject("load child '"+_2c+"' error.");
});
return _32.promise;
},_getViewNamesFromDefaults:function(_38){
var _39=_38.parent;
var _3a=_38.name;
var _3b="";
while(_39!==this.app){
_3a=_39.name+","+_3a;
_39=_39.parent;
}
var _3c=_38.defaultView.split("+");
for(var _3d in _3c){
_3c[_3d]=_3a+","+_3c[_3d];
}
_3b=_3c.join("+");
return _3b;
}});
});
