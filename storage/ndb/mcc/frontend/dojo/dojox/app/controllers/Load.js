//>>built
define("dojox/app/controllers/Load",["dojo/_base/lang","dojo/_base/declare","dojo/on","dojo/Deferred","dojo/when","../Controller","../View"],function(_1,_2,on,_3,_4,_5,_6){
return _2("dojox.app.controllers.Load",_5,{constructor:function(_7,_8){
this.events={"load":this.load};
this.inherited(arguments);
},load:function(_9){
var _a=_9.parent||this.app;
var _b=_9.viewId||"";
var _c=_b.split(",");
var _d=_c.shift();
var _e=_c.join(",");
var _f=_9.params||"";
var def=this.loadChild(_a,_d,_e,_f);
if(_9.callback){
_4(def,_9.callback);
}
return def;
},createChild:function(_10,_11,_12,_13){
var id=_10.id+"_"+_11;
if(_10.children[id]){
return _10.children[id];
}
var _14=new _6(_1.mixin({"app":this.app,"id":id,"name":_11,"parent":_10},{"params":_13}));
_10.children[id]=_14;
return _14.start();
},loadChild:function(_15,_16,_17,_18){
if(!_15){
throw Error("No parent for Child '"+_16+"'.");
}
if(!_16){
var _19=_15.defaultView?_15.defaultView.split(","):"default";
_16=_19.shift();
_17=_19.join(",");
}
var _1a=new _3();
var _1b;
try{
_1b=this.createChild(_15,_16,_17,_18);
}
catch(ex){
_1a.reject("load child '"+_16+"' error.");
return _1a.promise;
}
_4(_1b,_1.hitch(this,function(_1c){
if(!_17&&_1c.defaultView){
_17=_1c.defaultView;
}
var _1d=_17.split(",");
_16=_1d.shift();
_17=_1d.join(",");
if(_16){
var _1e=this.loadChild(_1c,_16,_17,_18);
_4(_1e,function(){
_1a.resolve();
},function(){
_1a.reject("load child '"+_16+"' error.");
});
}else{
_1a.resolve();
}
}),function(){
_1a.reject("load child '"+_16+"' error.");
});
return _1a.promise;
}});
});
