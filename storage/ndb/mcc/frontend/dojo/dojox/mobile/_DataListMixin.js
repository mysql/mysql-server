//>>built
define("dojox/mobile/_DataListMixin",["dojo/_base/array","dojo/_base/declare","dijit/registry","./_DataMixin","./ListItem","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/_StoreListMixin"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_2(_6("dojo-bidi")?"dojox.mobile._NonBidiDataListMixin":"dojox.mobile._DataListMixin",_4,{append:false,itemMap:null,itemRenderer:_5,buildRendering:function(){
this.inherited(arguments);
if(!this.store){
return;
}
var _9=this.store;
this.store=null;
this.setStore(_9,this.query,this.queryOptions);
},createListItem:function(_a){
var _b={};
var _c=this.store.getLabelAttributes(_a);
var _d=_c?_c[0]:null;
_1.forEach(this.store.getAttributes(_a),function(_e){
if(_e===_d){
_b["label"]=this.store.getLabel(_a);
}else{
_b[(this.itemMap&&this.itemMap[_e])||_e]=this.store.getValue(_a,_e);
}
},this);
if(_6("dojo-bidi")&&typeof _b["dir"]=="undefined"){
_b["dir"]=this.isLeftToRight()?"ltr":"rtl";
}
var w=new this.itemRenderer(_b);
_a._widgetId=w.id;
return w;
},generateList:function(_f,_10){
if(!this.append){
_1.forEach(this.getChildren(),function(_11){
_11.destroyRecursive();
});
}
_1.forEach(_f,function(_12,_13){
this.addChild(this.createListItem(_12));
},this);
},onComplete:function(_14,_15){
this.generateList(_14,_15);
},onError:function(_16,_17){
},onSet:function(_18,_19,_1a,_1b){
},onNew:function(_1c,_1d){
this.addChild(this.createListItem(_1c));
},onDelete:function(_1e){
_3.byId(_1e._widgetId).destroyRecursive();
},onStoreClose:function(_1f){
if(this.store.clearOnClose){
this.refresh();
}
}});
return _6("dojo-bidi")?_2("dojox.mobile._DataListMixin",[_8,_7]):_8;
});
