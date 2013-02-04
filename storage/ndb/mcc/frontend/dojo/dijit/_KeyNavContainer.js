//>>built
define("dijit/_KeyNavContainer",["dojo/_base/kernel","./_Container","./_FocusMixin","dojo/_base/array","dojo/keys","dojo/_base/declare","dojo/_base/event","dojo/dom-attr","dojo/_base/lang"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _6("dijit._KeyNavContainer",[_3,_2],{tabIndex:"0",connectKeyNavHandlers:function(_a,_b){
var _c=(this._keyNavCodes={});
var _d=_9.hitch(this,"focusPrev");
var _e=_9.hitch(this,"focusNext");
_4.forEach(_a,function(_f){
_c[_f]=_d;
});
_4.forEach(_b,function(_10){
_c[_10]=_e;
});
_c[_5.HOME]=_9.hitch(this,"focusFirstChild");
_c[_5.END]=_9.hitch(this,"focusLastChild");
this.connect(this.domNode,"onkeypress","_onContainerKeypress");
this.connect(this.domNode,"onfocus","_onContainerFocus");
},startupKeyNavChildren:function(){
_1.deprecated("startupKeyNavChildren() call no longer needed","","2.0");
},startup:function(){
this.inherited(arguments);
_4.forEach(this.getChildren(),_9.hitch(this,"_startupChild"));
},addChild:function(_11,_12){
this.inherited(arguments);
this._startupChild(_11);
},focus:function(){
this.focusFirstChild();
},focusFirstChild:function(){
this.focusChild(this._getFirstFocusableChild());
},focusLastChild:function(){
this.focusChild(this._getLastFocusableChild());
},focusNext:function(){
this.focusChild(this._getNextFocusableChild(this.focusedChild,1));
},focusPrev:function(){
this.focusChild(this._getNextFocusableChild(this.focusedChild,-1),true);
},focusChild:function(_13,_14){
if(!_13){
return;
}
if(this.focusedChild&&_13!==this.focusedChild){
this._onChildBlur(this.focusedChild);
}
_13.set("tabIndex",this.tabIndex);
_13.focus(_14?"end":"start");
this._set("focusedChild",_13);
},_startupChild:function(_15){
_15.set("tabIndex","-1");
this.connect(_15,"_onFocus",function(){
_15.set("tabIndex",this.tabIndex);
});
this.connect(_15,"_onBlur",function(){
_15.set("tabIndex","-1");
});
},_onContainerFocus:function(evt){
if(evt.target!==this.domNode||this.focusedChild){
return;
}
this.focusFirstChild();
_8.set(this.domNode,"tabIndex","-1");
},_onBlur:function(evt){
if(this.tabIndex){
_8.set(this.domNode,"tabIndex",this.tabIndex);
}
this.focusedChild=null;
this.inherited(arguments);
},_onContainerKeypress:function(evt){
if(evt.ctrlKey||evt.altKey){
return;
}
var _16=this._keyNavCodes[evt.charOrCode];
if(_16){
_16();
_7.stop(evt);
}
},_onChildBlur:function(){
},_getFirstFocusableChild:function(){
return this._getNextFocusableChild(null,1);
},_getLastFocusableChild:function(){
return this._getNextFocusableChild(null,-1);
},_getNextFocusableChild:function(_17,dir){
if(_17){
_17=this._getSiblingOfChild(_17,dir);
}
var _18=this.getChildren();
for(var i=0;i<_18.length;i++){
if(!_17){
_17=_18[(dir>0)?0:(_18.length-1)];
}
if(_17.isFocusable()){
return _17;
}
_17=this._getSiblingOfChild(_17,dir);
}
return null;
}});
});
