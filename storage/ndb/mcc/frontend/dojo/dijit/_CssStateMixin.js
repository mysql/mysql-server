//>>built
define("dijit/_CssStateMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom","dojo/dom-class","dojo/has","dojo/_base/lang","dojo/on","dojo/ready","dojo/_base/window","./registry"],function(_1,_2,_3,_4,_5,_6,on,_7,_8,_9){
var _a=_2("dijit._CssStateMixin",[],{cssStateNodes:{},hovering:false,active:false,_applyAttributes:function(){
this.inherited(arguments);
_1.forEach(["disabled","readOnly","checked","selected","focused","state","hovering","active","_opened"],function(_b){
this.watch(_b,_6.hitch(this,"_setStateClass"));
},this);
for(var ap in this.cssStateNodes){
this._trackMouseState(this[ap],this.cssStateNodes[ap]);
}
this._trackMouseState(this.domNode,this.baseClass);
this._setStateClass();
},_cssMouseEvent:function(_c){
if(!this.disabled){
switch(_c.type){
case "mouseover":
this._set("hovering",true);
this._set("active",this._mouseDown);
break;
case "mouseout":
this._set("hovering",false);
this._set("active",false);
break;
case "mousedown":
case "touchstart":
this._set("active",true);
break;
case "mouseup":
case "touchend":
this._set("active",false);
break;
}
}
},_setStateClass:function(){
var _d=this.baseClass.split(" ");
function _e(_f){
_d=_d.concat(_1.map(_d,function(c){
return c+_f;
}),"dijit"+_f);
};
if(!this.isLeftToRight()){
_e("Rtl");
}
var _10=this.checked=="mixed"?"Mixed":(this.checked?"Checked":"");
if(this.checked){
_e(_10);
}
if(this.state){
_e(this.state);
}
if(this.selected){
_e("Selected");
}
if(this._opened){
_e("Opened");
}
if(this.disabled){
_e("Disabled");
}else{
if(this.readOnly){
_e("ReadOnly");
}else{
if(this.active){
_e("Active");
}else{
if(this.hovering){
_e("Hover");
}
}
}
}
if(this.focused){
_e("Focused");
}
var tn=this.stateNode||this.domNode,_11={};
_1.forEach(tn.className.split(" "),function(c){
_11[c]=true;
});
if("_stateClasses" in this){
_1.forEach(this._stateClasses,function(c){
delete _11[c];
});
}
_1.forEach(_d,function(c){
_11[c]=true;
});
var _12=[];
for(var c in _11){
_12.push(c);
}
tn.className=_12.join(" ");
this._stateClasses=_d;
},_subnodeCssMouseEvent:function(_13,_14,evt){
if(this.disabled||this.readOnly){
return;
}
function _15(_16){
_4.toggle(_13,_14+"Hover",_16);
};
function _17(_18){
_4.toggle(_13,_14+"Active",_18);
};
function _19(_1a){
_4.toggle(_13,_14+"Focused",_1a);
};
switch(evt.type){
case "mouseover":
_15(true);
break;
case "mouseout":
_15(false);
_17(false);
break;
case "mousedown":
case "touchstart":
_17(true);
break;
case "mouseup":
case "touchend":
_17(false);
break;
case "focus":
case "focusin":
_19(true);
break;
case "blur":
case "focusout":
_19(false);
break;
}
},_trackMouseState:function(_1b,_1c){
_1b._cssState=_1c;
}});
_7(function(){
function _1d(evt){
if(!_3.isDescendant(evt.relatedTarget,evt.target)){
for(var _1e=evt.target;_1e&&_1e!=evt.relatedTarget;_1e=_1e.parentNode){
if(_1e._cssState){
var _1f=_9.getEnclosingWidget(_1e);
if(_1f){
if(_1e==_1f.domNode){
_1f._cssMouseEvent(evt);
}else{
_1f._subnodeCssMouseEvent(_1e,_1e._cssState,evt);
}
}
}
}
}
};
function _20(evt){
evt.target=evt.srcElement;
_1d(evt);
};
var _21=_8.body(),_22=(_5("touch")?[]:["mouseover","mouseout"]).concat(["mousedown","touchstart","mouseup","touchend"]);
_1.forEach(_22,function(_23){
if(_21.addEventListener){
_21.addEventListener(_23,_1d,true);
}else{
_21.attachEvent("on"+_23,_20);
}
});
on(_21,"focusin, focusout",function(evt){
var _24=evt.target;
if(_24._cssState&&!_24.getAttribute("widgetId")){
var _25=_9.getEnclosingWidget(_24);
_25._subnodeCssMouseEvent(_24,_24._cssState,evt);
}
});
});
return _a;
});
