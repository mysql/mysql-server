//>>built
define("dojox/mobile/CheckBox",["dojo/_base/declare","dojo/dom-construct","dijit/form/_CheckBoxMixin","./ToggleButton"],function(_1,_2,_3,_4){
return _1("dojox.mobile.CheckBox",[_4,_3],{baseClass:"mblCheckBox",_setTypeAttr:function(){
},buildRendering:function(){
if(!this.srcNodeRef){
this.srcNodeRef=_2.create("input",{type:this.type});
}
this.inherited(arguments);
this.focusNode=this.domNode;
},_getValueAttr:function(){
return (this.checked?this.value:false);
}});
});
