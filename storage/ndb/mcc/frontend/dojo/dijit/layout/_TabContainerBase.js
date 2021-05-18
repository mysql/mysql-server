//>>built
require({cache:{"url:dijit/layout/templates/TabContainer.html":"<div class=\"dijitTabContainer\">\n\t<div class=\"dijitTabListWrapper\" data-dojo-attach-point=\"tablistNode\"></div>\n\t<div data-dojo-attach-point=\"tablistSpacer\" class=\"dijitTabSpacer ${baseClass}-spacer\"></div>\n\t<div class=\"dijitTabPaneWrapper ${baseClass}-container\" data-dojo-attach-point=\"containerNode\"></div>\n</div>\n"}});
define("dijit/layout/_TabContainerBase",["dojo/_base/declare","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","./StackContainer","./utils","../_TemplatedMixin","dojo/text!./templates/TabContainer.html"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dijit.layout._TabContainerBase",[_5,_7],{tabPosition:"top",baseClass:"dijitTabContainer",tabStrip:false,nested:false,templateString:_8,postMixInProperties:function(){
this.baseClass+=this.tabPosition.charAt(0).toUpperCase()+this.tabPosition.substr(1).replace(/-.*/,"");
this.srcNodeRef&&_4.set(this.srcNodeRef,"visibility","hidden");
this.inherited(arguments);
},buildRendering:function(){
this.inherited(arguments);
this.tablist=this._makeController(this.tablistNode);
if(!this.doLayout){
_2.add(this.domNode,"dijitTabContainerNoLayout");
}
if(this.nested){
_2.add(this.domNode,"dijitTabContainerNested");
_2.add(this.tablist.containerNode,"dijitTabContainerTabListNested");
_2.add(this.tablistSpacer,"dijitTabContainerSpacerNested");
_2.add(this.containerNode,"dijitTabPaneWrapperNested");
}else{
_2.add(this.domNode,"tabStrip-"+(this.tabStrip?"enabled":"disabled"));
}
},_setupChild:function(_9){
_2.add(_9.domNode,"dijitTabPane");
this.inherited(arguments);
},removeChild:function(_a){
_2.remove(_a.domNode,"dijitTabPane");
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
this.tablist.startup();
this.inherited(arguments);
},layout:function(){
if(!this._contentBox||typeof (this._contentBox.l)=="undefined"){
return;
}
var sc=this.selectedChildWidget;
if(this.doLayout){
var _b=this.tabPosition.replace(/-h/,"");
this.tablist.region=_b;
var _c=[this.tablist,{domNode:this.tablistSpacer,region:_b},{domNode:this.containerNode,region:"center"}];
_6.layoutChildren(this.domNode,this._contentBox,_c);
this._containerContentBox=_6.marginBox2contentBox(this.containerNode,_c[2]);
if(sc&&sc.resize){
sc.resize(this._containerContentBox);
}
}else{
if(this.tablist.resize){
var s=this.tablist.domNode.style;
s.width="0";
var _d=_3.getContentBox(this.domNode).w;
s.width="";
this.tablist.resize({w:_d});
}
if(sc&&sc.resize){
sc.resize();
}
}
},destroy:function(_e){
if(this.tablist){
this.tablist.destroy(_e);
}
this.inherited(arguments);
}});
});
