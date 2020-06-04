//>>built
define("dojox/calendar/StoreManager",["dojo/_base/declare","dojo/_base/array","dojo/_base/html","dojo/_base/lang","dojo/dom-class","dojo/Stateful","dojo/Evented","dojo/when"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.calendar.StoreManager",[_6,_7],{owner:null,store:null,_ownerItemsProperty:null,_getParentStoreManager:function(){
if(this.owner&&this.owner.owner){
return this.owner.owner.get("storeManager");
}
return null;
},_initItems:function(_9){
this.set("items",_9);
return _9;
},_itemsSetter:function(_a){
this.items=_a;
this.emit("dataLoaded",_a);
},_computeVisibleItems:function(_b){
var _c=_b.startTime;
var _d=_b.endTime;
var _e=null;
var _f=this.owner[this._ownerItemsProperty];
if(_f){
_e=_2.filter(_f,function(_10){
return this.owner.isOverlapping(_b,_10.startTime,_10.endTime,_c,_d);
},this);
}
return _e;
},_updateItems:function(_11,_12,_13){
var _14=true;
var _15=null;
var _16=this.owner.itemToRenderItem(_11,this.store);
_16._item=_11;
this.items=this.owner[this._ownerItemsProperty];
if(_12!==-1){
if(_13!==_12){
this.items.splice(_12,1);
if(this.owner.setItemSelected&&this.owner.isItemSelected(_16)){
this.owner.setItemSelected(_16,false);
this.owner.dispatchChange(_16,this.get("selectedItem"),null,null);
}
}else{
_15=this.items[_12];
var cal=this.owner.dateModule;
_14=cal.compare(_16.startTime,_15.startTime)!==0||cal.compare(_16.endTime,_15.endTime)!==0;
_4.mixin(_15,_16);
}
}else{
if(_13!==-1){
var l,i;
var _17=_11.temporaryId;
if(_17){
l=this.items?this.items.length:0;
for(i=l-1;i>=0;i--){
if(this.items[i].id===_17){
this.items[i]=_16;
break;
}
}
var _18=this._getItemStoreStateObj({id:_17});
this._cleanItemStoreState(_17);
this._setItemStoreState(_16,_18?_18.state:null);
}
var s=this._getItemStoreStateObj(_16);
if(s&&s.state==="storing"){
if(this.items&&this.items[_13]&&this.items[_13].id!==_16.id){
l=this.items.length;
for(i=l-1;i>=0;i--){
if(this.items[i].id===_16.id){
this.items.splice(i,1);
break;
}
}
this.items.splice(_13,0,_16);
}
_4.mixin(s.renderItem,_16);
}else{
this.items.splice(_13,0,_16);
}
this.set("items",this.items);
}
}
this._setItemStoreState(_16,"stored");
if(!this.owner._isEditing){
if(_14){
this.emit("layoutInvalidated");
}else{
this.emit("renderersInvalidated",_15);
}
}
},_storeSetter:function(_19){
var r;
var _1a=this.owner;
if(this._observeHandler){
this._observeHandler.remove();
this._observeHandler=null;
}
if(_19){
var _1b=_19.query(_1a.query,_1a.queryOptions);
if(_1b.observe){
this._observeHandler=_1b.observe(_4.hitch(this,this._updateItems),true);
}
_1b=_1b.map(_4.hitch(this,function(_1c){
var _1d=_1a.itemToRenderItem(_1c,_19);
if(_1d.id==null){
console.err("The data item "+_1c.summary+" must have an unique identifier from the store.getIdentity(). The calendar will NOT work properly.");
}
_1d._item=_1c;
return _1d;
}));
r=_8(_1b,_4.hitch(this,this._initItems));
}else{
r=this._initItems([]);
}
this.store=_19;
return r;
},_getItemStoreStateObj:function(_1e){
var _1f=this._getParentStoreManager();
if(_1f){
return _1f._getItemStoreStateObj(_1e);
}
var _20=this.get("store");
if(_20!=null&&this._itemStoreState!=null){
var id=_1e.id===undefined?_20.getIdentity(_1e):_1e.id;
return this._itemStoreState[id];
}
return null;
},getItemStoreState:function(_21){
var _22=this._getParentStoreManager();
if(_22){
return _22.getItemStoreState(_21);
}
if(this._itemStoreState==null){
return "stored";
}
var _23=this.get("store");
var id=_21.id===undefined?_23.getIdentity(_21):_21.id;
var s=this._itemStoreState[id];
if(_23!=null&&s!==undefined){
return s.state;
}
return "stored";
},_cleanItemStoreState:function(id){
var _24=this._getParentStoreManager();
if(_24){
return _24._cleanItemStoreState(id);
}
if(!this._itemStoreState){
return;
}
var s=this._itemStoreState[id];
if(s){
delete this._itemStoreState[id];
return true;
}
return false;
},_setItemStoreState:function(_25,_26){
var _27=this._getParentStoreManager();
if(_27){
_27._setItemStoreState(_25,_26);
return;
}
if(this._itemStoreState===undefined){
this._itemStoreState={};
}
var _28=this.get("store");
var id=_25.id===undefined?_28.getIdentity(_25):_25.id;
var s=this._itemStoreState[id];
if(_26==="stored"||_26==null){
if(s!==undefined){
delete this._itemStoreState[id];
}
return;
}
if(_28){
this._itemStoreState[id]={id:id,item:_25,renderItem:this.owner.itemToRenderItem(_25,_28),state:_26};
}
}});
});
