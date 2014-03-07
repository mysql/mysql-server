//>>built
define("dojox/mvc/_patches",["dojo/_base/lang","dojo/_base/array","dijit/_WidgetBase","./_DataBindingMixin","dijit/form/ValidationTextBox","dijit/form/NumberTextBox"],function(_1,_2,wb,_3,_4,_5){
_1.extend(wb,new _3());
var _6=wb.prototype.startup;
wb.prototype.startup=function(){
this._dbstartup();
_6.apply(this);
};
var _7=wb.prototype.destroy;
wb.prototype.destroy=function(_8){
if(this._modelWatchHandles){
_2.forEach(this._modelWatchHandles,function(h){
h.unwatch();
});
}
if(this._viewWatchHandles){
_2.forEach(this._viewWatchHandles,function(h){
h.unwatch();
});
}
_7.apply(this,[_8]);
};
var _9=_4.prototype.isValid;
_4.prototype.isValid=function(_a){
return (this.inherited("isValid",arguments)!==false&&_9.apply(this,[_a]));
};
var _b=_5.prototype.isValid;
_5.prototype.isValid=function(_c){
return (this.inherited("isValid",arguments)!==false&&_b.apply(this,[_c]));
};
});
