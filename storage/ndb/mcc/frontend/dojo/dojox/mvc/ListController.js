//>>built
define("dojox/mvc/ListController",["dojo/_base/array","dojo/_base/lang","dojo/_base/declare","./ModelRefController"],function(_1,_2,_3,_4){
function _5(c){
for(var s in {"_listModelWatchHandle":1,"_tableModelWatchHandle":1}){
if(c[s]){
c[s].unwatch();
c[s]=null;
}
}
};
function _6(_7,_8,_9){
_5(_7);
if(_9&&_8!==_9){
if(_9.watchElements){
_7._listModelWatchHandle=_9.watchElements(function(_a,_b,_c){
if(_b&&_c){
var _d=_7.get("cursorIndex");
if(_b&&_d>=_a&&_d<_a+_b.length){
_7.set("cursorIndex",-1);
return;
}
if((_b.length||_c.length)&&_d>=_a){
_7.set(_7._refCursorProp,_7.get("cursor"));
}
}else{
_7.set(_7._refCursorProp,_7.get(_7._refCursorProp));
}
});
}else{
if(_9.set&&_9.watch){
if(_7.get("cursorIndex")<0){
_7._set("cursorIndex","");
}
_7._tableModelWatchHandle=_9.watch(function(_e,_f,_10){
if(_f!==_10&&_e==_7.get("cursorIndex")){
_7.set(_7._refCursorProp,_10);
}
});
}
}
}
_7._setCursorIndexAttr(_7.cursorIndex);
};
function _11(_12,old,_13){
var _14=_12[_12._refInModelProp];
if(!_14){
return;
}
if(old!==_13){
if(_2.isArray(_14)){
var _15=_1.indexOf(_14,_13);
if(_15<0){
var _16=_12.get("cursorIndex");
if(_16>=0&&_16<_14.length){
_14.set(_16,_13);
}
}else{
_12.set("cursorIndex",_15);
}
}else{
for(var s in _14){
if(_14[s]==_13){
_12.set("cursorIndex",s);
return;
}
}
var _16=_12.get("cursorIndex");
if(_16){
_14.set(_16,_13);
}
}
}
};
return _3("dojox.mvc.ListController",_4,{idProperty:"uniqueId",cursorId:null,cursorIndex:-1,cursor:null,model:null,_listModelWatchHandle:null,_tableModelWatchHandle:null,_refCursorProp:"cursor",_refModelProp:"cursor",destroy:function(){
_5(this);
this.inherited(arguments);
},set:function(_17,_18){
var _19=this[this._refCursorProp];
var _1a=this[this._refInModelProp];
this.inherited(arguments);
if(_17==this._refCursorProp){
_11(this,_19,_18);
}
if(_17==this._refInModelProp){
_6(this,_1a,_18);
}
},_setCursorIdAttr:function(_1b){
var old=this.cursorId;
this._set("cursorId",_1b);
var _1c=this[this._refInModelProp];
if(!_1c){
return;
}
if(old!==_1b){
if(_2.isArray(_1c)){
for(var i=0;i<_1c.length;i++){
if(_1c[i][this.idProperty]==_1b){
this.set("cursorIndex",i);
return;
}
}
this._set("cursorIndex",-1);
}else{
for(var s in _1c){
if(_1c[s][this.idProperty]==_1b){
this.set("cursorIndex",s);
return;
}
}
this._set("cursorIndex","");
}
}
},_setCursorIndexAttr:function(_1d){
this._set("cursorIndex",_1d);
if(!this[this._refInModelProp]){
return;
}
this.set(this._refCursorProp,this[this._refInModelProp][_1d]);
this.set("cursorId",this[this._refInModelProp][_1d]&&this[this._refInModelProp][_1d][this.idProperty]);
},hasControllerProperty:function(_1e){
return this.inherited(arguments)||_1e==this._refCursorProp;
}});
});
