//>>built
define("dojox/mobile/CheckBox",["dojo/_base/declare","dojo/dom-construct","dijit/form/_CheckBoxMixin","./ToggleButton","./sniff"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.CheckBox",[_4,_3],{baseClass:"mblCheckBox",_setTypeAttr:function(){
},buildRendering:function(){
if(!this.templateString&&!this.srcNodeRef){
this.srcNodeRef=_2.create("input",{type:this.type});
}
this.inherited(arguments);
if(!this.templateString){
this.focusNode=this.domNode;
}
if(_5("windows-theme")){
var _6=_2.create("span",{className:"mblCheckableInputContainer"});
_6.appendChild(this.domNode.cloneNode());
this.labelNode=_2.create("span",{className:"mblCheckableInputDecorator"},_6);
this.domNode=_6;
this.focusNode=_6.firstChild;
}
},_getValueAttr:function(){
return (this.checked?this.value:false);
}});
});
