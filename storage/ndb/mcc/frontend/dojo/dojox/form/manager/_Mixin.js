//>>built
define("dojox/form/manager/_Mixin",["dojo/_base/window","dojo/_base/lang","dojo/_base/array","dojo/on","dojo/dom-attr","dojo/dom-class","dijit/_base/manager","dijit/_Widget","dijit/form/_FormWidget","dijit/form/Button","dijit/form/CheckBox","dojo/_base/declare"],function(_1,_2,_3,on,_4,_5,_6,_7,_8,_9,_a,_b){
var fm=_2.getObject("dojox.form.manager",true),aa=fm.actionAdapter=function(_c){
return function(_d,_e,_f){
if(_2.isArray(_e)){
_3.forEach(_e,function(_10){
_c.call(this,_d,_10,_f);
},this);
}else{
_c.apply(this,arguments);
}
};
},ia=fm.inspectorAdapter=function(_11){
return function(_12,_13,_14){
return _11.call(this,_12,_2.isArray(_13)?_13[0]:_13,_14);
};
},_15={domNode:1,containerNode:1,srcNodeRef:1,bgIframe:1},_16=fm._keys=function(o){
var _17=[],key;
for(key in o){
if(o.hasOwnProperty(key)){
_17.push(key);
}
}
return _17;
},_18=function(_19){
var _1a=_19.get("name");
if(_1a&&_19.isInstanceOf(_8)){
if(_1a in this.formWidgets){
var a=this.formWidgets[_1a].widget;
if(_2.isArray(a)){
a.push(_19);
}else{
this.formWidgets[_1a].widget=[a,_19];
}
}else{
this.formWidgets[_1a]={widget:_19,connections:[]};
}
}else{
_1a=null;
}
return _1a;
},_1b=function(_1c){
var _1d={};
aa(function(_1e,w){
var o=w.get("data-dojo-observer")||w.get("observer");
if(o&&typeof o=="string"){
_3.forEach(o.split(","),function(o){
o=_2.trim(o);
if(o&&_2.isFunction(this[o])){
_1d[o]=1;
}
},this);
}
}).call(this,null,this.formWidgets[_1c].widget);
return _16(_1d);
},_1f=function(_20,_21){
var t=this.formWidgets[_20],w=t.widget,c=t.connections;
if(c.length){
_3.forEach(c,function(_22){
_22.remove();
});
c=t.connections=[];
}
if(_2.isArray(w)){
_3.forEach(w,function(w){
_3.forEach(_21,function(o){
c.push(on(w,"change",_2.hitch(this,function(evt){
if(this.watching&&_4.get(w.focusNode,"checked")){
this[o](w.get("value"),_20,w,evt);
}
})));
},this);
},this);
}else{
var _23=w.isInstanceOf(_9)?"click":"change";
_3.forEach(_21,function(o){
c.push(on(w,_23,_2.hitch(this,function(evt){
if(this.watching){
this[o](w.get("value"),_20,w,evt);
}
})));
},this);
}
};
var _24=_b("dojox.form.manager._Mixin",null,{watching:true,startup:function(){
if(this._started){
return;
}
this.formWidgets={};
this.formNodes={};
this.registerWidgetDescendants(this);
this.inherited(arguments);
},destroy:function(){
for(var _25 in this.formWidgets){
_3.forEach(this.formWidgets[_25].connections,function(_26){
_26.remove();
});
}
this.formWidgets={};
this.inherited(arguments);
},registerWidget:function(_27){
if(typeof _27=="string"){
_27=_6.byId(_27);
}else{
if(_27.tagName&&_27.cloneNode){
_27=_6.byNode(_27);
}
}
var _28=_18.call(this,_27);
if(_28){
_1f.call(this,_28,_1b.call(this,_28));
}
return this;
},unregisterWidget:function(_29){
if(_29 in this.formWidgets){
_3.forEach(this.formWidgets[_29].connections,function(_2a){
_2a.remove();
});
delete this.formWidgets[_29];
}
return this;
},registerWidgetDescendants:function(_2b){
if(typeof _2b=="string"){
_2b=_6.byId(_2b);
}else{
if(_2b.tagName&&_2b.cloneNode){
_2b=_6.byNode(_2b);
}
}
var _2c=_3.map(_2b.getDescendants(),_18,this);
_3.forEach(_2c,function(_2d){
if(_2d){
_1f.call(this,_2d,_1b.call(this,_2d));
}
},this);
return this.registerNodeDescendants?this.registerNodeDescendants(_2b.domNode):this;
},unregisterWidgetDescendants:function(_2e){
if(typeof _2e=="string"){
_2e=_6.byId(_2e);
}else{
if(_2e.tagName&&_2e.cloneNode){
_2e=_6.byNode(_2e);
}
}
_3.forEach(_3.map(_2e.getDescendants(),function(w){
return w instanceof _8&&w.get("name")||null;
}),function(_2f){
if(_2f){
this.unregisterWidget(_2f);
}
},this);
return this.unregisterNodeDescendants?this.unregisterNodeDescendants(_2e.domNode):this;
},formWidgetValue:function(_30,_31){
var _32=arguments.length==2&&_31!==undefined,_33;
if(typeof _30=="string"){
_30=this.formWidgets[_30];
if(_30){
_30=_30.widget;
}
}
if(!_30){
return null;
}
if(_2.isArray(_30)){
if(_32){
_3.forEach(_30,function(_34){
_34.set("checked",false,!this.watching);
},this);
_3.forEach(_30,function(_35){
_35.set("checked",_35.value===_31,!this.watching);
},this);
return this;
}
_3.some(_30,function(_36){
if(_4.get(_36.focusNode,"checked")){
_33=_36;
return true;
}
return false;
});
return _33?_33.get("value"):"";
}
if(_30.isInstanceOf&&_30.isInstanceOf(_a)){
if(_32){
_30.set("value",Boolean(_31),!this.watching);
return this;
}
return Boolean(_30.get("value"));
}
if(_32){
_30.set("value",_31,!this.watching);
return this;
}
return _30.get("value");
},formPointValue:function(_37,_38){
if(_37&&typeof _37=="string"){
_37=this[_37];
}
if(!_37||!_37.tagName||!_37.cloneNode){
return null;
}
if(!_5.contains(_37,"dojoFormValue")){
return null;
}
if(arguments.length==2&&_38!==undefined){
_37.innerHTML=_38;
return this;
}
return _37.innerHTML;
},inspectFormWidgets:function(_39,_3a,_3b){
var _3c,_3d={};
if(_3a){
if(_2.isArray(_3a)){
_3.forEach(_3a,function(_3e){
if(_3e in this.formWidgets){
_3d[_3e]=_39.call(this,_3e,this.formWidgets[_3e].widget,_3b);
}
},this);
}else{
for(_3c in _3a){
if(_3c in this.formWidgets){
_3d[_3c]=_39.call(this,_3c,this.formWidgets[_3c].widget,_3a[_3c]);
}
}
}
}else{
for(_3c in this.formWidgets){
_3d[_3c]=_39.call(this,_3c,this.formWidgets[_3c].widget,_3b);
}
}
return _3d;
},inspectAttachedPoints:function(_3f,_40,_41){
var _42,_43,_44={};
if(_40){
if(_2.isArray(_40)){
_3.forEach(_40,function(_45){
_43=this[_45];
if(_43&&_43.tagName&&_43.cloneNode){
_44[_45]=_3f.call(this,_45,_43,_41);
}
},this);
}else{
for(_42 in _40){
_43=this[_42];
if(_43&&_43.tagName&&_43.cloneNode){
_44[_42]=_3f.call(this,_42,_43,_40[_42]);
}
}
}
}else{
for(_42 in this){
if(!(_42 in _15)){
_43=this[_42];
if(_43&&_43.tagName&&_43.cloneNode){
_44[_42]=_3f.call(this,_42,_43,_41);
}
}
}
}
return _44;
},inspect:function(_46,_47,_48){
var _49=this.inspectFormWidgets(function(_4a,_4b,_4c){
if(_2.isArray(_4b)){
return _46.call(this,_4a,_3.map(_4b,function(w){
return w.domNode;
}),_4c);
}
return _46.call(this,_4a,_4b.domNode,_4c);
},_47,_48);
if(this.inspectFormNodes){
_2.mixin(_49,this.inspectFormNodes(_46,_47,_48));
}
return _2.mixin(_49,this.inspectAttachedPoints(_46,_47,_48));
}});
_2.extend(_7,{observer:""});
return _24;
});
