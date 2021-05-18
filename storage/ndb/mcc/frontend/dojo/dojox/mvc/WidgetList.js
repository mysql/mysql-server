//>>built
define("dojox/mvc/WidgetList",["require","dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dijit/_Container","dijit/_WidgetBase","./Templated"],function(_1,_2,_3,_4,_5,_6,_7){
var _8="data-mvc-child-type",_9="data-mvc-child-mixins",_a="data-mvc-child-props",_b="data-mvc-child-bindings",_c;
function _d(_e){
return eval("({"+_e+"})");
};
function _f(w){
for(var h=null;h=(w._handles||[]).pop();){
h.unwatch();
}
};
function _10(a){
var _11=[];
_2.forEach(a,function(_12){
[].push.apply(_11,_12);
});
return _11;
};
function _13(_14,_15){
if(this.childClz){
_15(this.childClz);
}else{
if(this.childType){
var _16=!_3.isFunction(this.childType)&&!_3.isFunction(this.childMixins)?[[this.childType].concat(this.childMixins&&this.childMixins.split(",")||[])]:_2.map(_14,function(_17){
var _18=_3.isFunction(this.childType)?this.childType.call(_17,this):this.childType,_19=_3.isFunction(this.childMixins)?this.childMixins.call(_17,this):this.childMixins;
return _18?[_18].concat(_3.isArray(_19)?_19:_19?_19.split(","):[]):["dojox/mvc/Templated"];
},this);
_1(_2.filter(_2.map(_10(_16),function(_1a){
return _3.getObject(_1a)?_c:_1a;
}),function(_1b){
return _1b!==_c;
}),function(){
_15.apply(this,_2.map(_16,function(_1c){
var _1d=_2.map(_1c,function(_1e){
return _3.getObject(_1e)||_1(_1e);
});
return _1d.length>1?_4(_1d,{}):_1d[0];
}));
});
}else{
_15(_7);
}
}
};
var _1f=_4("dojox.mvc.WidgetList",[_6,_5],{childClz:null,childType:"",childMixins:"",childParams:null,childBindings:null,children:null,partialRebuild:false,_relTargetProp:"children",postMixInProperties:function(){
this.inherited(arguments);
if(this[_8]){
this.childType=this[_8];
}
if(this[_9]){
this.childMixins=this[_9];
}
},startup:function(){
this.inherited(arguments);
this._setChildrenAttr(this.children);
},_setChildrenAttr:function(_20){
var _21=this.children;
this._set("children",_20);
if(this._started&&(!this._builtOnce||_21!=_20)){
this._builtOnce=true;
this._buildChildren(_20);
if(_3.isArray(_20)){
var _22=this;
_20.watch!=={}.watch&&(this._handles=this._handles||[]).push(_20.watch(function(_23,old,_24){
if(!isNaN(_23)){
var w=_22.getChildren()[_23-0];
w&&w.set(w._relTargetProp||"target",_24);
}
}));
}
}
},_buildChildren:function(_25){
_f(this);
for(var cw=this.getChildren(),w=null;w=cw.pop();){
this.removeChild(w);
w.destroy();
}
if(!_3.isArray(_25)){
return;
}
var _26=this,seq=this._buildChildrenSeq=(this._buildChildrenSeq||0)+1,_27={idx:0,removals:[],adds:[].concat(_25)},_28=[_27];
function _29(_2a){
if(this._beingDestroyed||this._buildChildrenSeq>seq){
return;
}
var _2b=[].slice.call(arguments,1);
_2a.clz=_3.isFunction(this.childType)||_3.isFunction(this.childMixins)?_2b:_2b[0];
for(var _2c=null;_2c=_28.shift();){
if(!_2c.clz){
_28.unshift(_2c);
break;
}
for(var i=0,l=(_2c.removals||[]).length;i<l;++i){
this.removeChild(_2c.idx);
}
_2.forEach(_2.map(_2c.adds,function(_2d,idx){
var _2e={ownerDocument:this.ownerDocument,parent:this,indexAtStartup:_2c.idx+idx},_2f=_3.isArray(_2c.clz)?_2c.clz[idx]:_2c.clz;
_2e[(_3.isFunction(this.childParams)&&this.childParams.call(_2e,this)||this.childParams||this[_a]&&_d.call(_2e,this[_a])||{})._relTargetProp||_2f.prototype._relTargetProp||"target"]=_2d;
var _30=this.childParams||this[_a]&&_d.call(_2e,this[_a]),_31=this.childBindings||this[_b]&&_d.call(_2e,this[_b]);
if(this.templateString&&!_2e.templateString&&!_2f.prototype.templateString){
_2e.templateString=this.templateString;
}
if(_31&&!_2e.bindings&&!_2f.prototype.bindings){
_2e.bindings=_31;
}
return new _2f(_3.delegate(_3.isFunction(_30)?_30.call(_2e,this):_30,_2e));
},this),function(_32,idx){
this.addChild(_32,_2c.idx+idx);
},this);
}
};
_3.isFunction(_25.watchElements)&&(this._handles=this._handles||[]).push(_25.watchElements(function(idx,_33,_34){
if(!_33||!_34||!_26.partialRebuild){
_26._buildChildren(_25);
}else{
var _35={idx:idx,removals:_33,adds:_34};
_28.push(_35);
_13.call(_26,_34,_3.hitch(_26,_29,_35));
}
}));
_13.call(this,_25,_3.hitch(this,_29,_27));
},destroy:function(){
_f(this);
this.inherited(arguments);
}});
_1f.prototype[_8]=_1f.prototype[_9]=_1f.prototype[_a]=_1f.prototype[_b]="";
return _1f;
});
