//>>built
require({cache:{"url:dijit/templates/MenuItem.html":"<tr class=\"dijitReset dijitMenuItem\" data-dojo-attach-point=\"focusNode\" role=\"menuitem\" tabIndex=\"-1\">\n\t<td class=\"dijitReset dijitMenuItemIconCell\" role=\"presentation\">\n\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitIcon dijitMenuItemIcon\" data-dojo-attach-point=\"iconNode\"/>\n\t</td>\n\t<td class=\"dijitReset dijitMenuItemLabel\" colspan=\"2\" data-dojo-attach-point=\"containerNode\"></td>\n\t<td class=\"dijitReset dijitMenuItemAccelKey\" style=\"display: none\" data-dojo-attach-point=\"accelKeyNode\"></td>\n\t<td class=\"dijitReset dijitMenuArrowCell\" role=\"presentation\">\n\t\t<div data-dojo-attach-point=\"arrowWrapper\" style=\"visibility: hidden\">\n\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitMenuExpand\"/>\n\t\t\t<span class=\"dijitMenuExpandA11y\">+</span>\n\t\t</div>\n\t</td>\n</tr>\n"}});
define("dijit/MenuItem",["dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/_base/kernel","dojo/sniff","./_Widget","./_TemplatedMixin","./_Contained","./_CssStateMixin","dojo/text!./templates/MenuItem.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _1("dijit.MenuItem",[_7,_8,_9,_a],{templateString:_b,baseClass:"dijitMenuItem",label:"",_setLabelAttr:function(_c){
this.containerNode.innerHTML=_c;
this._set("label",_c);
if(this.textDir==="auto"){
this.applyTextDir(this.focusNode,this.label);
}
},iconClass:"dijitNoIcon",_setIconClassAttr:{node:"iconNode",type:"class"},accelKey:"",disabled:false,_fillContent:function(_d){
if(_d&&!("label" in this.params)){
this.set("label",_d.innerHTML);
}
},buildRendering:function(){
this.inherited(arguments);
var _e=this.id+"_text";
_3.set(this.containerNode,"id",_e);
if(this.accelKeyNode){
_3.set(this.accelKeyNode,"id",this.id+"_accel");
_e+=" "+this.id+"_accel";
}
this.domNode.setAttribute("aria-labelledby",_e);
_2.setSelectable(this.domNode,false);
},onClick:function(){
},focus:function(){
try{
if(_6("ie")==8){
this.containerNode.focus();
}
this.focusNode.focus();
}
catch(e){
}
},_onFocus:function(){
this._setSelected(true);
this.getParent()._onItemFocus(this);
this.inherited(arguments);
},_setSelected:function(_f){
_4.toggle(this.domNode,"dijitMenuItemSelected",_f);
},setLabel:function(_10){
_5.deprecated("dijit.MenuItem.setLabel() is deprecated.  Use set('label', ...) instead.","","2.0");
this.set("label",_10);
},setDisabled:function(_11){
_5.deprecated("dijit.Menu.setDisabled() is deprecated.  Use set('disabled', bool) instead.","","2.0");
this.set("disabled",_11);
},_setDisabledAttr:function(_12){
this.focusNode.setAttribute("aria-disabled",_12?"true":"false");
this._set("disabled",_12);
},_setAccelKeyAttr:function(_13){
this.accelKeyNode.style.display=_13?"":"none";
this.accelKeyNode.innerHTML=_13;
_3.set(this.containerNode,"colSpan",_13?"1":"2");
this._set("accelKey",_13);
},_setTextDirAttr:function(_14){
if(!this._created||this.textDir!=_14){
this._set("textDir",_14);
this.applyTextDir(this.focusNode,this.label);
}
}});
});
