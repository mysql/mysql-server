//>>built
define("dojox/calendar/StoreMixin",["dojo/_base/declare","dojo/_base/array","dojo/_base/html","dojo/_base/lang","dojo/dom-class","dojo/Stateful","dojo/when"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dojox.calendar.StoreMixin",_6,{store:null,query:{},queryOptions:null,startTimeAttr:"startTime",endTimeAttr:"endTime",summaryAttr:"summary",allDayAttr:"allDay",subColumnAttr:"calendar",cssClassFunc:null,decodeDate:null,encodeDate:null,displayedItemsInvalidated:false,itemToRenderItem:function(_8,_9){
if(this.owner){
return this.owner.itemToRenderItem(_8,_9);
}
return {id:_9.getIdentity(_8),summary:_8[this.summaryAttr],startTime:(this.decodeDate&&this.decodeDate(_8[this.startTimeAttr]))||this.newDate(_8[this.startTimeAttr],this.dateClassObj),endTime:(this.decodeDate&&this.decodeDate(_8[this.endTimeAttr]))||this.newDate(_8[this.endTimeAttr],this.dateClassObj),allDay:_8[this.allDayAttr]!=null?_8[this.allDayAttr]:false,subColumn:_8[this.subColumnAttr],cssClass:this.cssClassFunc?this.cssClassFunc(_8):null};
},renderItemToItem:function(_a,_b){
if(this.owner){
return this.owner.renderItemToItem(_a,_b);
}
var _c={};
_c[_b.idProperty]=_a.id;
_c[this.summaryAttr]=_a.summary;
_c[this.startTimeAttr]=(this.encodeDate&&this.encodeDate(_a.startTime))||_a.startTime;
_c[this.endTimeAttr]=(this.encodeDate&&this.encodeDate(_a.endTime))||_a.endTime;
if(_a.subColumn){
_c[this.subColumnAttr]=_a.subColumn;
}
return this.getItemStoreState(_a)==="unstored"?_c:_4.mixin(_a._item,_c);
},_computeVisibleItems:function(_d){
if(this.owner){
return this.owner._computeVisibleItems(_d);
}
_d.items=this.storeManager._computeVisibleItems(_d);
},_initItems:function(_e){
this.set("items",_e);
return _e;
},_refreshItemsRendering:function(_f){
},_setStoreAttr:function(_10){
this.store=_10;
return this.storeManager.set("store",_10);
},_getItemStoreStateObj:function(_11){
return this.storeManager._getItemStoreStateObj(_11);
},getItemStoreState:function(_12){
return this.storeManager.getItemStoreState(_12);
},_cleanItemStoreState:function(id){
this.storeManager._cleanItemStoreState(id);
},_setItemStoreState:function(_13,_14){
this.storeManager._setItemStoreState(_13,_14);
}});
});
