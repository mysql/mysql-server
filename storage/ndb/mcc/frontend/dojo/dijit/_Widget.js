//>>built
define("dijit/_Widget",["dojo/aspect","dojo/_base/config","dojo/_base/connect","dojo/_base/declare","dojo/_base/kernel","dojo/_base/lang","dojo/query","dojo/ready","./registry","./_WidgetBase","./_OnDijitClickMixin","./_FocusMixin","dojo/uacss","./hccss"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
function _d(){
};
function _e(_f){
return function(obj,_10,_11,_12){
if(obj&&typeof _10=="string"&&obj[_10]==_d){
return obj.on(_10.substring(2).toLowerCase(),_6.hitch(_11,_12));
}
return _f.apply(_3,arguments);
};
};
_1.around(_3,"connect",_e);
if(_5.connect){
_1.around(_5,"connect",_e);
}
var _13=_4("dijit._Widget",[_a,_b,_c],{onClick:_d,onDblClick:_d,onKeyDown:_d,onKeyPress:_d,onKeyUp:_d,onMouseDown:_d,onMouseMove:_d,onMouseOut:_d,onMouseOver:_d,onMouseLeave:_d,onMouseEnter:_d,onMouseUp:_d,constructor:function(_14){
this._toConnect={};
for(var _15 in _14){
if(this[_15]===_d){
this._toConnect[_15.replace(/^on/,"").toLowerCase()]=_14[_15];
delete _14[_15];
}
}
},postCreate:function(){
this.inherited(arguments);
for(var _16 in this._toConnect){
this.on(_16,this._toConnect[_16]);
}
delete this._toConnect;
},on:function(_17,_18){
if(this[this._onMap(_17)]===_d){
return _3.connect(this.domNode,_17.toLowerCase(),this,_18);
}
return this.inherited(arguments);
},_setFocusedAttr:function(val){
this._focused=val;
this._set("focused",val);
},setAttribute:function(_19,_1a){
_5.deprecated(this.declaredClass+"::setAttribute(attr, value) is deprecated. Use set() instead.","","2.0");
this.set(_19,_1a);
},attr:function(_1b,_1c){
if(_2.isDebug){
var _1d=arguments.callee._ach||(arguments.callee._ach={}),_1e=(arguments.callee.caller||"unknown caller").toString();
if(!_1d[_1e]){
_5.deprecated(this.declaredClass+"::attr() is deprecated. Use get() or set() instead, called from "+_1e,"","2.0");
_1d[_1e]=true;
}
}
var _1f=arguments.length;
if(_1f>=2||typeof _1b==="object"){
return this.set.apply(this,arguments);
}else{
return this.get(_1b);
}
},getDescendants:function(){
_5.deprecated(this.declaredClass+"::getDescendants() is deprecated. Use getChildren() instead.","","2.0");
return this.containerNode?_7("[widgetId]",this.containerNode).map(_9.byNode):[];
},_onShow:function(){
this.onShow();
},onShow:function(){
},onHide:function(){
},onClose:function(){
return true;
}});
if(!_5.isAsync){
_8(0,function(){
var _20=["dijit/_base"];
require(_20);
});
}
return _13;
});
