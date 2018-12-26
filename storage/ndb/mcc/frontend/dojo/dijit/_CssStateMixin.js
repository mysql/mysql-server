//>>built
define("dijit/_CssStateMixin",["dojo/touch","dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/_base/lang","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6){
return _3("dijit._CssStateMixin",[],{cssStateNodes:{},hovering:false,active:false,_applyAttributes:function(){
this.inherited(arguments);
_2.forEach(["onmouseenter","onmouseleave",_1.press],function(e){
this.connect(this.domNode,e,"_cssMouseEvent");
},this);
_2.forEach(["disabled","readOnly","checked","selected","focused","state","hovering","active"],function(_7){
this.watch(_7,_5.hitch(this,"_setStateClass"));
},this);
for(var ap in this.cssStateNodes){
this._trackMouseState(this[ap],this.cssStateNodes[ap]);
}
this._setStateClass();
},_cssMouseEvent:function(_8){
if(!this.disabled){
switch(_8.type){
case "mouseenter":
case "mouseover":
this._set("hovering",true);
this._set("active",this._mouseDown);
break;
case "mouseleave":
case "mouseout":
this._set("hovering",false);
this._set("active",false);
break;
case "mousedown":
case "touchpress":
this._set("active",true);
this._mouseDown=true;
var _9=this.connect(_6.body(),_1.release,function(){
this._mouseDown=false;
this._set("active",false);
this.disconnect(_9);
});
break;
}
}
},_setStateClass:function(){
var _a=this.baseClass.split(" ");
function _b(_c){
_a=_a.concat(_2.map(_a,function(c){
return c+_c;
}),"dijit"+_c);
};
if(!this.isLeftToRight()){
_b("Rtl");
}
var _d=this.checked=="mixed"?"Mixed":(this.checked?"Checked":"");
if(this.checked){
_b(_d);
}
if(this.state){
_b(this.state);
}
if(this.selected){
_b("Selected");
}
if(this.disabled){
_b("Disabled");
}else{
if(this.readOnly){
_b("ReadOnly");
}else{
if(this.active){
_b("Active");
}else{
if(this.hovering){
_b("Hover");
}
}
}
}
if(this.focused){
_b("Focused");
}
var tn=this.stateNode||this.domNode,_e={};
_2.forEach(tn.className.split(" "),function(c){
_e[c]=true;
});
if("_stateClasses" in this){
_2.forEach(this._stateClasses,function(c){
delete _e[c];
});
}
_2.forEach(_a,function(c){
_e[c]=true;
});
var _f=[];
for(var c in _e){
_f.push(c);
}
tn.className=_f.join(" ");
this._stateClasses=_a;
},_trackMouseState:function(_10,_11){
var _12=false,_13=false,_14=false;
var _15=this,cn=_5.hitch(this,"connect",_10);
function _16(){
var _17=("disabled" in _15&&_15.disabled)||("readonly" in _15&&_15.readonly);
_4.toggle(_10,_11+"Hover",_12&&!_13&&!_17);
_4.toggle(_10,_11+"Active",_13&&!_17);
_4.toggle(_10,_11+"Focused",_14&&!_17);
};
cn("onmouseenter",function(){
_12=true;
_16();
});
cn("onmouseleave",function(){
_12=false;
_13=false;
_16();
});
cn(_1.press,function(){
_13=true;
_16();
});
cn(_1.release,function(){
_13=false;
_16();
});
cn("onfocus",function(){
_14=true;
_16();
});
cn("onblur",function(){
_14=false;
_16();
});
this.watch("disabled",_16);
this.watch("readOnly",_16);
}});
});
