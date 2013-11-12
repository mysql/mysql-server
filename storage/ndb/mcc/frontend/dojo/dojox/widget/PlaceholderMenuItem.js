//>>built
define("dojox/widget/PlaceholderMenuItem",["dojo","dijit","dojox","dijit/Menu","dijit/MenuItem"],function(_1,_2,_3){
_1.experimental("dojox.widget.PlaceholderMenuItem");
_1.declare("dojox.widget.PlaceholderMenuItem",_2.MenuItem,{_replaced:false,_replacedWith:null,_isPlaceholder:true,postCreate:function(){
this.domNode.style.display="none";
this._replacedWith=[];
if(!this.label){
this.label=this.containerNode.innerHTML;
}
this.inherited(arguments);
},replace:function(_4){
if(this._replaced){
return false;
}
var _5=this.getIndexInParent();
if(_5<0){
return false;
}
var p=this.getParent();
_1.forEach(_4,function(_6){
p.addChild(_6,_5++);
});
this._replacedWith=_4;
this._replaced=true;
return true;
},unReplace:function(_7){
if(!this._replaced){
return [];
}
var p=this.getParent();
if(!p){
return [];
}
var r=this._replacedWith;
_1.forEach(this._replacedWith,function(_8){
p.removeChild(_8);
if(_7){
_8.destroyRecursive();
}
});
this._replacedWith=[];
this._replaced=false;
return r;
}});
_1.extend(_2.Menu,{getPlaceholders:function(_9){
var r=[];
var _a=this.getChildren();
_1.forEach(_a,function(_b){
if(_b._isPlaceholder&&(!_9||_b.label==_9)){
r.push(_b);
}else{
if(_b._started&&_b.popup&&_b.popup.getPlaceholders){
r=r.concat(_b.popup.getPlaceholders(_9));
}else{
if(!_b._started&&_b.dropDownContainer){
var _c=_1.query("[widgetId]",_b.dropDownContainer)[0];
var _d=_2.byNode(_c);
if(_d.getPlaceholders){
r=r.concat(_d.getPlaceholders(_9));
}
}
}
}
},this);
return r;
}});
return _3.widget.PlaceholderMenuItem;
});
