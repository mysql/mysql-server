//>>built
define("dojox/form/manager/_Mixin",["dojo/_base/window","dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/dom-attr","dojo/dom-class","dijit/_base/manager","dijit/_Widget","dijit/form/_FormWidget","dijit/form/Button","dijit/form/CheckBox","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var fm=_2.getObject("dojox.form.manager",true),aa=fm.actionAdapter=function(_d){
return function(_e,_f,_10){
if(_2.isArray(_f)){
_3.forEach(_f,function(_11){
_d.call(this,_e,_11,_10);
},this);
}else{
_d.apply(this,arguments);
}
};
},ia=fm.inspectorAdapter=function(_12){
return function(_13,_14,_15){
return _12.call(this,_13,_2.isArray(_14)?_14[0]:_14,_15);
};
},_16={domNode:1,containerNode:1,srcNodeRef:1,bgIframe:1},_17=fm._keys=function(o){
var _18=[],key;
for(key in o){
if(o.hasOwnProperty(key)){
_18.push(key);
}
}
return _18;
},_19=function(_1a){
var _1b=_1a.get("name");
if(_1b&&_1a instanceof _9){
if(_1b in this.formWidgets){
var a=this.formWidgets[_1b].widget;
if(_2.isArray(a)){
a.push(_1a);
}else{
this.formWidgets[_1b].widget=[a,_1a];
}
}else{
this.formWidgets[_1b]={widget:_1a,connections:[]};
}
}else{
_1b=null;
}
return _1b;
},_1c=function(_1d){
var _1e={};
aa(function(_1f,w){
var o=w.get("observer");
if(o&&typeof o=="string"){
_3.forEach(o.split(","),function(o){
o=_2.trim(o);
if(o&&_2.isFunction(this[o])){
_1e[o]=1;
}
},this);
}
}).call(this,null,this.formWidgets[_1d].widget);
return _17(_1e);
},_20=function(_21,_22){
var t=this.formWidgets[_21],w=t.widget,c=t.connections;
if(c.length){
_3.forEach(c,_4.disconnect);
c=t.connections=[];
}
if(_2.isArray(w)){
_3.forEach(w,function(w){
_3.forEach(_22,function(o){
c.push(_4.connect(w,"onChange",this,function(evt){
if(this.watching&&_5.get(w.focusNode,"checked")){
this[o](w.get("value"),_21,w,evt);
}
}));
},this);
},this);
}else{
var _23=w.isInstanceOf(_a)?"onClick":"onChange";
_3.forEach(_22,function(o){
c.push(_4.connect(w,_23,this,function(evt){
if(this.watching){
this[o](w.get("value"),_21,w,evt);
}
}));
},this);
}
};
var _24=_c("dojox.form.manager._Mixin",null,{watching:true,startup:function(){
if(this._started){
return;
}
this.formWidgets={};
this.formNodes={};
this.registerWidgetDescendants(this);
this.inherited(arguments);
},destroy:function(){
for(var _25 in this.formWidgets){
_3.forEach(this.formWidgets[_25].connections,_4.disconnect);
}
this.formWidgets={};
this.inherited(arguments);
},registerWidget:function(_26){
if(typeof _26=="string"){
_26=_7.byId(_26);
}else{
if(_26.tagName&&_26.cloneNode){
_26=_7.byNode(_26);
}
}
var _27=_19.call(this,_26);
if(_27){
_20.call(this,_27,_1c.call(this,_27));
}
return this;
},unregisterWidget:function(_28){
if(_28 in this.formWidgets){
_3.forEach(this.formWidgets[_28].connections,this.disconnect,this);
delete this.formWidgets[_28];
}
return this;
},registerWidgetDescendants:function(_29){
if(typeof _29=="string"){
_29=_7.byId(_29);
}else{
if(_29.tagName&&_29.cloneNode){
_29=_7.byNode(_29);
}
}
var _2a=_3.map(_29.getDescendants(),_19,this);
_3.forEach(_2a,function(_2b){
if(_2b){
_20.call(this,_2b,_1c.call(this,_2b));
}
},this);
return this.registerNodeDescendants?this.registerNodeDescendants(_29.domNode):this;
},unregisterWidgetDescendants:function(_2c){
if(typeof _2c=="string"){
_2c=_7.byId(_2c);
}else{
if(_2c.tagName&&_2c.cloneNode){
_2c=_7.byNode(_2c);
}
}
_3.forEach(_3.map(_2c.getDescendants(),function(w){
return w instanceof _9&&w.get("name")||null;
}),function(_2d){
if(_2d){
this.unregisterWidget(_2d);
}
},this);
return this.unregisterNodeDescendants?this.unregisterNodeDescendants(_2c.domNode):this;
},formWidgetValue:function(_2e,_2f){
var _30=arguments.length==2&&_2f!==undefined,_31;
if(typeof _2e=="string"){
_2e=this.formWidgets[_2e];
if(_2e){
_2e=_2e.widget;
}
}
if(!_2e){
return null;
}
if(_2.isArray(_2e)){
if(_30){
_3.forEach(_2e,function(_32){
_32.set("checked",false,!this.watching);
},this);
_3.forEach(_2e,function(_33){
_33.set("checked",_33.value===_2f,!this.watching);
},this);
return this;
}
_3.some(_2e,function(_34){
if(_5.get(_34.focusNode,"checked")){
_31=_34;
return true;
}
return false;
});
return _31?_31.get("value"):"";
}
if(_2e.isInstanceOf&&_2e.isInstanceOf(_b)){
if(_30){
_2e.set("value",Boolean(_2f),!this.watching);
return this;
}
return Boolean(_2e.get("value"));
}
if(_30){
_2e.set("value",_2f,!this.watching);
return this;
}
return _2e.get("value");
},formPointValue:function(_35,_36){
if(_35&&typeof _35=="string"){
_35=this[_35];
}
if(!_35||!_35.tagName||!_35.cloneNode){
return null;
}
if(!_6.contains(_35,"dojoFormValue")){
return null;
}
if(arguments.length==2&&_36!==undefined){
_35.innerHTML=_36;
return this;
}
return _35.innerHTML;
},inspectFormWidgets:function(_37,_38,_39){
var _3a,_3b={};
if(_38){
if(_2.isArray(_38)){
_3.forEach(_38,function(_3c){
if(_3c in this.formWidgets){
_3b[_3c]=_37.call(this,_3c,this.formWidgets[_3c].widget,_39);
}
},this);
}else{
for(_3a in _38){
if(_3a in this.formWidgets){
_3b[_3a]=_37.call(this,_3a,this.formWidgets[_3a].widget,_38[_3a]);
}
}
}
}else{
for(_3a in this.formWidgets){
_3b[_3a]=_37.call(this,_3a,this.formWidgets[_3a].widget,_39);
}
}
return _3b;
},inspectAttachedPoints:function(_3d,_3e,_3f){
var _40,_41={};
if(_3e){
if(_2.isArray(_3e)){
_3.forEach(_3e,function(_42){
var _43=this[_42];
if(_43&&_43.tagName&&_43.cloneNode){
_41[_42]=_3d.call(this,_42,_43,_3f);
}
},this);
}else{
for(_40 in _3e){
var _44=this[_40];
if(_44&&_44.tagName&&_44.cloneNode){
_41[_40]=_3d.call(this,_40,_44,_3e[_40]);
}
}
}
}else{
for(_40 in this){
if(!(_40 in _16)){
var _44=this[_40];
if(_44&&_44.tagName&&_44.cloneNode){
_41[_40]=_3d.call(this,_40,_44,_3f);
}
}
}
}
return _41;
},inspect:function(_45,_46,_47){
var _48=this.inspectFormWidgets(function(_49,_4a,_4b){
if(_2.isArray(_4a)){
return _45.call(this,_49,_3.map(_4a,function(w){
return w.domNode;
}),_4b);
}
return _45.call(this,_49,_4a.domNode,_4b);
},_46,_47);
if(this.inspectFormNodes){
_2.mixin(_48,this.inspectFormNodes(_45,_46,_47));
}
return _2.mixin(_48,this.inspectAttachedPoints(_45,_46,_47));
}});
_2.extend(_8,{observer:""});
return _24;
});
