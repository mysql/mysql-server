//>>built
define("dojox/mobile/_ListTouchMixin",["dojo/_base/declare","dojo/_base/event","dijit/form/_ListBase"],function(_1,_2,_3){
return _1("dojox.mobile._ListTouchMixin",_3,{postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onclick","_onClick");
},_onClick:function(_4){
_2.stop(_4);
var _5=this._getTarget(_4);
if(_5){
this._setSelectedAttr(_5);
this.onClick(_5);
}
}});
});
