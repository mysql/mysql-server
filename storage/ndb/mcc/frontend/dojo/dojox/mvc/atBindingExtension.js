//>>built
define("dojox/mvc/atBindingExtension",["dojo/aspect","dojo/_base/array","dojo/_base/lang","dijit/_WidgetBase","./_atBindingMixin","dijit/registry"],function(_1,_2,_3,_4,_5){
return function(w){
_2.forEach(arguments,function(w){
if(w.dataBindAttr){
console.warn("Detected a widget or a widget class that has already been applied data binding extension. Skipping...");
return;
}
_3._mixin(w,_5.mixin);
_1.before(w,"postscript",function(_6,_7){
this._dbpostscript(_6,_7);
});
_1.before(w,"startup",function(){
if(this._started){
return;
}
this._startAtWatchHandles();
});
_1.before(w,"destroy",function(){
this._stopAtWatchHandles();
});
_1.around(w,"set",function(_8){
return function(_9,_a){
if(_9==_5.prototype.dataBindAttr){
return this._setBind(_a);
}else{
if((_a||{}).atsignature=="dojox.mvc.at"){
return this._setAtWatchHandle(_9,_a);
}
}
return _8.apply(this,_3._toArray(arguments));
};
});
});
return arguments;
};
});
