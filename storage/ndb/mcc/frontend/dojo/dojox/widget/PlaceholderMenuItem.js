//>>built
define("dojox/widget/PlaceholderMenuItem",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/dom-style","dojo/_base/kernel","dojo/query","dijit/registry","dijit/Menu","dijit/MenuItem"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
_5.experimental("dojox.widget.PlaceholderMenuItem");
var _a=_2("dojox.widget.PlaceholderMenuItem",_9,{_replaced:false,_replacedWith:null,_isPlaceholder:true,postCreate:function(){
_4.set(this.domNode,"display","none");
this._replacedWith=[];
if(!this.label){
this.label=this.containerNode.innerHTML;
}
this.inherited(arguments);
},replace:function(_b){
if(this._replaced){
return false;
}
var _c=this.getIndexInParent();
if(_c<0){
return false;
}
var p=this.getParent();
_1.forEach(_b,function(_d){
p.addChild(_d,_c++);
});
this._replacedWith=_b;
this._replaced=true;
return true;
},unReplace:function(_e){
if(!this._replaced){
return [];
}
var p=this.getParent();
if(!p){
return [];
}
var r=this._replacedWith;
_1.forEach(this._replacedWith,function(_f){
p.removeChild(_f);
if(_e){
_f.destroyRecursive();
}
});
this._replacedWith=[];
this._replaced=false;
return r;
}});
_3.extend(_8,{getPlaceholders:function(_10){
var r=[];
var _11=this.getChildren();
_1.forEach(_11,function(_12){
if(_12._isPlaceholder&&(!_10||_12.label==_10)){
r.push(_12);
}else{
if(_12._started&&_12.popup&&_12.popup.getPlaceholders){
r=r.concat(_12.popup.getPlaceholders(_10));
}else{
if(!_12._started&&_12.dropDownContainer){
var _13=_6("[widgetId]",_12.dropDownContainer)[0];
var _14=_7.byNode(_13);
if(_14.getPlaceholders){
r=r.concat(_14.getPlaceholders(_10));
}
}
}
}
},this);
return r;
}});
return _a;
});
