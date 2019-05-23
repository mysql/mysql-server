//>>built
define("dojox/mobile/_StoreListMixin",["dojo/_base/array","dojo/_base/declare","./_StoreMixin","./ListItem"],function(_1,_2,_3,_4){
return _2("dojox.mobile._StoreListMixin",_3,{append:false,itemMap:null,buildRendering:function(){
this.inherited(arguments);
if(!this.store){
return;
}
var _5=this.store;
this.store=null;
this.setStore(_5,this.query,this.queryOptions);
},createListItem:function(_6){
var _7={};
if(!_6["label"]){
_7["label"]=_6[this.labelProperty];
}
for(var _8 in _6){
_7[(this.itemMap&&this.itemMap[_8])||_8]=_6[_8];
}
return new _4(_7);
},generateList:function(_9){
if(!this.append){
_1.forEach(this.getChildren(),function(_a){
_a.destroyRecursive();
});
}
_1.forEach(_9,function(_b,_c){
this.addChild(this.createListItem(_b));
if(_b[this.childrenProperty]){
_1.forEach(_b[this.childrenProperty],function(_d,_e){
this.addChild(this.createListItem(_d));
},this);
}
},this);
},onComplete:function(_f){
this.generateList(_f);
},onError:function(){
},onUpdate:function(_10,_11){
if(_11===this.getChildren().length){
this.addChild(this.createListItem(_10));
}else{
this.getChildren()[_11].set(_10);
}
},onDelete:function(_12,_13){
this.getChildren()[_13].destroyRecursive();
}});
});
