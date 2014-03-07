//>>built
define("dojox/mobile/FixedSplitterPane",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dijit/_Contained","dijit/_Container","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5,_6){
return _2("dojox.mobile.FixedSplitterPane",[_6,_5,_4],{buildRendering:function(){
this.inherited(arguments);
_3.add(this.domNode,"mblFixedSplitterPane");
},resize:function(){
_1.forEach(this.getChildren(),function(_7){
if(_7.resize){
_7.resize();
}
});
}});
});
