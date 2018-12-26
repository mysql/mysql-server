//>>built
require({cache:{"url:dijit/templates/MenuItem.html":"<tr class=\"dijitReset dijitMenuItem\" data-dojo-attach-point=\"focusNode\" role=\"menuitem\" tabIndex=\"-1\"\n\t\tdata-dojo-attach-event=\"onmouseenter:_onHover,onmouseleave:_onUnhover,ondijitclick:_onClick\">\n\t<td class=\"dijitReset dijitMenuItemIconCell\" role=\"presentation\">\n\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitIcon dijitMenuItemIcon\" data-dojo-attach-point=\"iconNode\"/>\n\t</td>\n\t<td class=\"dijitReset dijitMenuItemLabel\" colspan=\"2\" data-dojo-attach-point=\"containerNode\"></td>\n\t<td class=\"dijitReset dijitMenuItemAccelKey\" style=\"display: none\" data-dojo-attach-point=\"accelKeyNode\"></td>\n\t<td class=\"dijitReset dijitMenuArrowCell\" role=\"presentation\">\n\t\t<div data-dojo-attach-point=\"arrowWrapper\" style=\"visibility: hidden\">\n\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitMenuExpand\"/>\n\t\t\t<span class=\"dijitMenuExpandA11y\">+</span>\n\t\t</div>\n\t</td>\n</tr>\n"}});
define("dijit/MenuItem",["dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/_base/event","dojo/_base/kernel","dojo/_base/sniff","./_Widget","./_TemplatedMixin","./_Contained","./_CssStateMixin","dojo/text!./templates/MenuItem.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _1("dijit.MenuItem",[_8,_9,_a,_b],{templateString:_c,baseClass:"dijitMenuItem",label:"",_setLabelAttr:{node:"containerNode",type:"innerHTML"},iconClass:"dijitNoIcon",_setIconClassAttr:{node:"iconNode",type:"class"},accelKey:"",disabled:false,_fillContent:function(_d){
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
},_onHover:function(){
this.getParent().onItemHover(this);
},_onUnhover:function(){
this.getParent().onItemUnhover(this);
this._set("hovering",false);
},_onClick:function(_f){
this.getParent().onItemClick(this,_f);
_5.stop(_f);
},onClick:function(){
},focus:function(){
try{
if(_7("ie")==8){
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
},_setSelected:function(_10){
_4.toggle(this.domNode,"dijitMenuItemSelected",_10);
},setLabel:function(_11){
_6.deprecated("dijit.MenuItem.setLabel() is deprecated.  Use set('label', ...) instead.","","2.0");
this.set("label",_11);
},setDisabled:function(_12){
_6.deprecated("dijit.Menu.setDisabled() is deprecated.  Use set('disabled', bool) instead.","","2.0");
this.set("disabled",_12);
},_setDisabledAttr:function(_13){
this.focusNode.setAttribute("aria-disabled",_13?"true":"false");
this._set("disabled",_13);
},_setAccelKeyAttr:function(_14){
this.accelKeyNode.style.display=_14?"":"none";
this.accelKeyNode.innerHTML=_14;
_3.set(this.containerNode,"colSpan",_14?"1":"2");
this._set("accelKey",_14);
}});
});
