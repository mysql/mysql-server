//>>built
define("dojox/mobile/_ComboBoxMenu",["dojo/_base/kernel","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dijit/form/_ComboBoxMenuMixin","dijit/_WidgetBase","./_ListTouchMixin","./scrollable"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.mobile._ComboBoxMenu",[_6,_7,_5],{baseClass:"mblComboBoxMenu",bgIframe:true,buildRendering:function(){
this.domNode=this.focusNode=_4.create("div",{"class":"mblReset"});
this.containerNode=_4.create("div",{style:{position:"absolute",top:0,left:0}},this.domNode);
this.previousButton=_4.create("div",{"class":"mblReset mblComboBoxMenuItem mblComboBoxMenuPreviousButton",role:"option"},this.containerNode);
this.nextButton=_4.create("div",{"class":"mblReset mblComboBoxMenuItem mblComboBoxMenuNextButton",role:"option"},this.containerNode);
this.inherited(arguments);
},_createMenuItem:function(){
return _4.create("div",{"class":"mblReset mblComboBoxMenuItem"+(this.isLeftToRight()?"":" mblComboBoxMenuItemRtl"),role:"option"});
},onSelect:function(_9){
_3.add(_9,"mblComboBoxMenuItemSelected");
},onDeselect:function(_a){
_3.remove(_a,"mblComboBoxMenuItemSelected");
},onOpen:function(){
this.scrollable.init({domNode:this.domNode,containerNode:this.containerNode});
this.scrollable.scrollTo({x:0,y:0});
},onClose:function(){
this.scrollable.cleanup();
},destroyRendering:function(){
this.bgIframe=false;
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this.scrollable=new _8();
this.scrollable.resize=function(){
};
}});
});
