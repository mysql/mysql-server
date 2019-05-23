//>>built
define("dojox/treemap/Keyboard",["dojo/_base/array","dojo/_base/lang","dojo/_base/event","dojo/_base/declare","dojo/on","dojo/keys","dojo/dom-attr","./_utils","dijit/_FocusMixin"],function(_1,_2,_3,_4,on,_5,_6,_7,_8){
return _4("dojox.treemap.Keyboard",_8,{tabIndex:"0",_setTabIndexAttr:"domNode",constructor:function(){
},postCreate:function(){
this.inherited(arguments);
this._keyDownHandle=on(this.domNode,"keydown",_2.hitch(this,this._onKeyDown));
this._mouseDownHandle=on(this.domNode,"mousedown",_2.hitch(this,this._onMouseDown));
},destroy:function(){
this.inherited(arguments);
this._keyDownHandle.remove();
this._mouseDownHandle.remove();
},createRenderer:function(_9,_a,_b){
var _c=this.inherited(arguments);
_6.set(_c,"tabindex","-1");
return _c;
},_onMouseDown:function(e){
this.domNode.focus();
},_onKeyDown:function(e){
var _d=this.get("selectedItem");
if(!_d){
return;
}
var _e=this.itemToRenderer[this.getIdentity(_d)];
var _f=_e.parentItem;
var _10,_11,_12;
if(e.keyCode!=_5.UP_ARROW&&e.keyCode!=_5.NUMPAD_MINUS&&e.keyCode!=_5.NUMPAD_PLUS){
_10=(e.keyCode==_5.DOWN_ARROW)?_d.children:_f.children;
if(_10){
_11=_7.initElements(_10,_2.hitch(this,this._computeAreaForItem)).elements;
_12=_11[_1.indexOf(_10,_d)];
_11.sort(function(a,b){
return b.size-a.size;
});
}else{
return;
}
}
var _13;
switch(e.keyCode){
case _5.LEFT_ARROW:
_13=_10[_11[Math.max(0,_1.indexOf(_11,_12)-1)].index];
break;
case _5.RIGHT_ARROW:
_13=_10[_11[Math.min(_11.length-1,_1.indexOf(_11,_12)+1)].index];
break;
case _5.DOWN_ARROW:
_13=_10[_11[0].index];
break;
case _5.UP_ARROW:
_13=_f;
break;
case _5.NUMPAD_PLUS:
if(!this._isLeaf(_d)&&this.drillDown){
this.drillDown(_e);
_3.stop(e);
}
break;
case _5.NUMPAD_MINUS:
if(!this._isLeaf(_d)&&this.drillUp){
this.drillUp(_e);
_3.stop(e);
}
break;
}
if(_13){
if(!this._isRoot(_13)){
this.set("selectedItem",_13);
_3.stop(e);
}
}
}});
});
