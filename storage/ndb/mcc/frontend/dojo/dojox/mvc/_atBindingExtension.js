//>>built
define("dojox/mvc/_atBindingExtension",["dojo/aspect","dojo/_base/lang","dijit/_WidgetBase","./_atBindingMixin","dijit/registry"],function(_1,_2,_3,_4){
_2.extend(_3,_4.prototype);
_1.before(_3.prototype,"postscript",function(_5,_6){
this._dbpostscript(_5,_6);
});
_1.before(_3.prototype,"startup",function(){
this._startAtWatchHandles();
});
_1.before(_3.prototype,"destroy",function(){
this._stopAtWatchHandles();
});
_1.around(_3.prototype,"set",function(_7){
return function(_8,_9){
if(_8==_4.prototype.dataBindAttr){
return this._setBind(_9);
}else{
if((_9||{}).atsignature=="dojox.mvc.at"){
return this._setAtWatchHandle(_8,_9);
}
}
return _7.apply(this,_2._toArray(arguments));
};
});
});
