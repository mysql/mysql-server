//>>built
define("dojox/mobile/_ListTouchMixin",["dojo/_base/declare","dojo/touch","./sniff","dijit/form/_ListBase"],function(_1,_2,_3,_4){
return _1("dojox.mobile._ListTouchMixin",_4,{postCreate:function(){
this.inherited(arguments);
if(!((_3("ie")===10||(!_3("ie")&&_3("trident")>6))&&typeof (MSGesture)!=="undefined")){
this._listConnect("click","_onClick");
}else{
this._listConnect(_2.press,"_onPress");
var _5=this,_6=new MSGesture(),_7;
this._onPress=function(e){
_6.target=_5.domNode;
_6.addPointer(e.pointerId);
_7=e.target;
};
this.on("MSGestureTap",function(e){
_5._onClick(e,_7);
});
}
},_onClick:function(_8,_9){
this._setSelectedAttr(_9);
this.onClick(_9);
}});
});
