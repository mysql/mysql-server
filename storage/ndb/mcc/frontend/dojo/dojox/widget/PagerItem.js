//>>built
define("dojox/widget/PagerItem",["dojo/_base/declare","dojo/dom-geometry","dojo/dom-style","dojo/parser","dijit/_WidgetBase","dijit/_TemplatedMixin"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.widget._PagerItem",[_5,_6],{templateString:"<li class=\"pagerItem\" data-dojo-attach-point=\"containerNode\"></li>",resizeChildren:function(){
var _7=_2.getMarginBox(this.containerNode);
_3.set(this.containerNode.firstChild,{width:_7.w+"px",height:_7.h+"px"});
},parseChildren:function(){
_4.parse(this.containerNode);
}});
});
