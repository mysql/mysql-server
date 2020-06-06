//>>built
define("dojox/mobile/ToggleButton",["dojo/_base/declare","dojo/dom-class","dijit/form/_ToggleButtonMixin","./Button"],function(_1,_2,_3,_4){
return _1("dojox.mobile.ToggleButton",[_4,_3],{baseClass:"mblToggleButton",_setCheckedAttr:function(){
this.inherited(arguments);
var _5=(this.baseClass+" "+this["class"]).replace(/(\S+)\s*/g,"$1Checked ").split(" ");
_2[this.checked?"add":"remove"](this.focusNode||this.domNode,_5);
}});
});
