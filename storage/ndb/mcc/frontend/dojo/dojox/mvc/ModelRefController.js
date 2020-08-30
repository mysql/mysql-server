//>>built
define("dojox/mvc/ModelRefController",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/Stateful","./_Controller"],function(_1,_2,_3,_4,_5){
return _2("dojox.mvc.ModelRefController",_5,{ownProps:null,_refModelProp:"model",_refInModelProp:"model",model:null,postscript:function(_6,_7){
this._relTargetProp=(_6||{})._refModelProp||this._refModelProp;
this.inherited(arguments);
},get:function(_8){
if(!this.hasControllerProperty(_8)){
var _9=this[this._refModelProp];
return !_9?void 0:_9.get?_9.get(_8):_9[_8];
}
return this.inherited(arguments);
},_set:function(_a,_b){
if(!this.hasControllerProperty(_a)){
var _c=this[this._refModelProp];
_c&&(_c.set?_c.set(_a,_b):(_c[_a]=_b));
return this;
}
return this.inherited(arguments);
},watch:function(_d,_e){
if(this.hasControllerProperty(_d)){
return this.inherited(arguments);
}
if(!_e){
_e=_d;
_d=null;
}
var hm=null,hp=null,_f=this;
function _10(_11){
if(hp){
hp.unwatch();
}
if(_11&&_3.isFunction(_11.set)&&_3.isFunction(_11.watch)){
hp=_11.watch.apply(_11,(_d?[_d]:[]).concat([function(_12,old,_13){
_e.call(_f,_12,old,_13);
}]));
}
};
function _14(old,_15){
var _16={};
if(!_d){
_1.forEach([old,_15],function(_17){
var _18=_17&&_17.get("properties");
if(_18){
_1.forEach(_18,function(_19){
if(!_f.hasControllerProperty(_19)){
_16[_19]=1;
}
});
}else{
for(var s in _17){
if(_17.hasOwnProperty(s)&&!_f.hasControllerProperty(s)){
_16[s]=1;
}
}
}
});
}else{
_16[_d]=1;
}
for(var s in _16){
_e.call(_f,s,!old?void 0:old.get?old.get(s):old[s],!_15?void 0:_15.get?_15.get(s):_15[s]);
}
};
hm=_4.prototype.watch.call(this,this._refModelProp,function(_1a,old,_1b){
if(old===_1b){
return;
}
_14(old,_1b);
_10(_1b);
});
_10(this.get(this._refModelProp));
var h={};
h.unwatch=h.remove=function(){
if(hp){
hp.unwatch();
hp=null;
}
if(hm){
hm.unwatch();
hm=null;
}
};
return h;
},hasControllerProperty:function(_1c){
return _1c=="_watchCallbacks"||_1c==this._refModelProp||_1c==this._refInModelProp||(_1c in (this.ownProps||{}))||(_1c in this.constructor.prototype)||/^dojoAttach(Point|Event)$/i.test(_1c);
}});
});
