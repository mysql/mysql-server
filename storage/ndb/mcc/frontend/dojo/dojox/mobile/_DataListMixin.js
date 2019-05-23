//>>built
define("dojox/mobile/_DataListMixin",["dojo/_base/array","dojo/_base/declare","dijit/registry","./_DataMixin","./ListItem"],function(_1,_2,_3,_4,_5){
return _2("dojox.mobile._DataListMixin",_4,{append:false,itemMap:null,buildRendering:function(){
this.inherited(arguments);
if(!this.store){
return;
}
var _6=this.store;
this.store=null;
this.setStore(_6,this.query,this.queryOptions);
},createListItem:function(_7){
var _8={};
var _9=this.store.getLabelAttributes(_7);
var _a=_9?_9[0]:null;
_1.forEach(this.store.getAttributes(_7),function(_b){
if(_b===_a){
_8["label"]=this.store.getLabel(_7);
}else{
_8[(this.itemMap&&this.itemMap[_b])||_b]=this.store.getValue(_7,_b);
}
},this);
var w=new _5(_8);
_7._widgetId=w.id;
return w;
},generateList:function(_c,_d){
if(!this.append){
_1.forEach(this.getChildren(),function(_e){
_e.destroyRecursive();
});
}
_1.forEach(_c,function(_f,_10){
this.addChild(this.createListItem(_f));
},this);
},onComplete:function(_11,_12){
this.generateList(_11,_12);
},onError:function(_13,_14){
},onSet:function(_15,_16,_17,_18){
},onNew:function(_19,_1a){
this.addChild(this.createListItem(_19));
},onDelete:function(_1b){
_3.byId(_1b._widgetId).destroyRecursive();
},onStoreClose:function(_1c){
if(this.store.clearOnClose){
this.refresh();
}
}});
});
