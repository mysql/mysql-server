//>>built
define("dojox/mobile/_ListTouchMixin",["dojo/_base/declare","dijit/form/_ListBase"],function(_1,_2){
return _1("dojox.mobile._ListTouchMixin",_2,{postCreate:function(){
this.inherited(arguments);
this._listConnect("click","_onClick");
},_onClick:function(_3,_4){
this._setSelectedAttr(_4);
this.onClick(_4);
}});
});
