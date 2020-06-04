//>>built
define("dojox/mobile/bidi/_ComboBoxMenu",["dojo/_base/declare","dojo/dom-construct","dojo/dom-class","dojo/dom-style"],function(_1,_2,_3,_4){
return _1(null,{buildRendering:function(){
this.inherited(arguments);
if(!this.isLeftToRight()){
this.containerNode.style.left="auto";
_4.set(this.containerNode,{position:"absolute",top:0,right:0});
_3.remove(this.previousButton,"mblComboBoxMenuItem");
_3.add(this.previousButton,"mblComboBoxMenuItemRtl");
_3.remove(this.nextButton,"mblComboBoxMenuItem");
_3.add(this.nextButton,"mblComboBoxMenuItemRtl");
}
}});
});
