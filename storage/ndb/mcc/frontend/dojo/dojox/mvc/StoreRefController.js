//>>built
define("dojox/mvc/StoreRefController",["dojo/_base/declare","dojo/_base/lang","dojo/when","./getStateful","./ModelRefController"],function(_1,_2,_3,_4,_5){
return _1("dojox.mvc.StoreRefController",_5,{store:null,getStatefulOptions:null,_refSourceModelProp:"model",queryStore:function(_6,_7){
if(!(this.store||{}).query){
return;
}
if(this._queryObserveHandle){
this._queryObserveHandle.cancel();
}
var _8=this,_9=this.store.query(_6,_7),_a=_3(_9,function(_b){
if(_8._beingDestroyed){
return;
}
_b=_4(_b,_8.getStatefulOptions);
_8.set(_8._refSourceModelProp,_b);
return _b;
});
if(_a.then){
_a=_2.delegate(_a);
}
for(var s in _9){
if(isNaN(s)&&_9.hasOwnProperty(s)&&_2.isFunction(_9[s])){
_a[s]=_9[s];
}
}
return _a;
},getStore:function(id,_c){
if(!(this.store||{}).get){
return;
}
if(this._queryObserveHandle){
this._queryObserveHandle.cancel();
}
var _d=this;
result=_3(this.store.get(id,_c),function(_e){
if(_d._beingDestroyed){
return;
}
_e=_4(_e,_d.getStatefulOptions);
_d.set(_d._refSourceModelProp,_e);
return _e;
});
return result;
},putStore:function(_f,_10){
if(!(this.store||{}).put){
return;
}
return this.store.put(_f,_10);
},addStore:function(_11,_12){
if(!(this.store||{}).add){
return;
}
return this.store.add(_11,_12);
},removeStore:function(id,_13){
if(!(this.store||{}).remove){
return;
}
return this.store.remove(id,_13);
}});
});
