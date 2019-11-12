//>>built
define("dojox/widget/PortletSettings",["dojo/_base/declare","dojo/_base/lang","dojo/dom-style","dojo/dom-class","dojo/fx","dijit/_Container","dijit/layout/ContentPane"],function(_1,_2,_3,_4,fx,_5,_6){
return _1("dojox.widget.PortletSettings",[_5,_6],{portletIconClass:"dojoxPortletSettingsIcon",portletIconHoverClass:"dojoxPortletSettingsIconHover",postCreate:function(){
_3.set(this.domNode,"display","none");
_4.add(this.domNode,"dojoxPortletSettingsContainer");
_4.remove(this.domNode,"dijitContentPane");
},_setPortletAttr:function(_7){
this.portlet=_7;
},toggle:function(){
var n=this.domNode;
if(_3.get(n,"display")=="none"){
_3.set(n,{"display":"block","height":"1px","width":"auto"});
fx.wipeIn({node:n}).play();
}else{
fx.wipeOut({node:n,onEnd:_2.hitch(this,function(){
_3.set(n,{"display":"none","height":"","width":""});
})}).play();
}
}});
});
