//>>built
define("dojox/mobile/_StoreListMixin",["dojo/_base/array","dojo/_base/declare","./_StoreMixin","./ListItem","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/_StoreListMixin"],function(_1,_2,_3,_4,_5,_6){
var _7=_2(_5("dojo-bidi")?"dojox.mobile._NonBidiStoreListMixin":"dojox.mobile._StoreListMixin",_3,{append:false,itemMap:null,itemRenderer:_4,buildRendering:function(){
this.inherited(arguments);
if(!this.store){
return;
}
var _8=this.store;
this.store=null;
this.setStore(_8,this.query,this.queryOptions);
},createListItem:function(_9){
return new this.itemRenderer(this._createItemProperties(_9));
},_createItemProperties:function(_a){
var _b={};
if(!_a["label"]){
_b["label"]=_a[this.labelProperty];
}
if(_5("dojo-bidi")&&typeof _b["dir"]=="undefined"){
_b["dir"]=this.isLeftToRight()?"ltr":"rtl";
}
for(var _c in _a){
_b[(this.itemMap&&this.itemMap[_c])||_c]=_a[_c];
}
return _b;
},_setDirAttr:function(_d){
return _d;
},generateList:function(_e){
if(!this.append){
_1.forEach(this.getChildren(),function(_f){
_f.destroyRecursive();
});
}
_1.forEach(_e,function(_10,_11){
this.addChild(this.createListItem(_10));
if(_10[this.childrenProperty]){
_1.forEach(_10[this.childrenProperty],function(_12,_13){
this.addChild(this.createListItem(_12));
},this);
}
},this);
},onComplete:function(_14){
this.generateList(_14);
},onError:function(){
},onAdd:function(_15,_16){
this.addChild(this.createListItem(_15),_16);
},onUpdate:function(_17,_18){
this.getChildren()[_18].set(this._createItemProperties(_17));
},onDelete:function(_19,_1a){
this.getChildren()[_1a].destroyRecursive();
}});
return _5("dojo-bidi")?_2("dojox.mobile._StoreListMixin",[_7,_6]):_7;
});
