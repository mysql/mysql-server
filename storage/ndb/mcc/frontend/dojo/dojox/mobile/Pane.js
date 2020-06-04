//>>built
define("dojox/mobile/Pane",["dojo/_base/array","dojo/_base/declare","dijit/_Contained","dijit/_WidgetBase"],function(_1,_2,_3,_4){
return _2("dojox.mobile.Pane",[_4,_3],{baseClass:"mblPane",buildRendering:function(){
this.inherited(arguments);
if(!this.containerNode){
this.containerNode=this.domNode;
}
},resize:function(){
_1.forEach(this.getChildren(),function(_5){
if(_5.resize){
_5.resize();
}
});
}});
});
