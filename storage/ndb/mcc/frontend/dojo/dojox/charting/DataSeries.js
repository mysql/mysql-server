//>>built
define("dojox/charting/DataSeries",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","dojox/lang/functional"],function(_1,_2,_3,_4,df){
return _2("dojox.charting.DataSeries",null,{constructor:function(_5,_6,_7){
this.store=_5;
this.kwArgs=_6;
if(_7){
if(_1.isFunction(_7)){
this.value=_7;
}else{
if(_1.isObject(_7)){
this.value=_1.hitch(this,"_dictValue",df.keys(_7),_7);
}else{
this.value=_1.hitch(this,"_fieldValue",_7);
}
}
}else{
this.value=_1.hitch(this,"_defaultValue");
}
this.data=[];
this._events=[];
if(this.store.getFeatures()["dojo.data.api.Notification"]){
this._events.push(_4.connect(this.store,"onNew",this,"_onStoreNew"),_4.connect(this.store,"onDelete",this,"_onStoreDelete"),_4.connect(this.store,"onSet",this,"_onStoreSet"));
}
this.fetch();
},destroy:function(){
_3.forEach(this._events,_4.disconnect);
},setSeriesObject:function(_8){
this.series=_8;
},_dictValue:function(_9,_a,_b,_c){
var o={};
_3.forEach(_9,function(_d){
o[_d]=_b.getValue(_c,_a[_d]);
});
return o;
},_fieldValue:function(_e,_f,_10){
return _f.getValue(_10,_e);
},_defaultValue:function(_11,_12){
return _11.getValue(_12,"value");
},fetch:function(){
if(!this._inFlight){
this._inFlight=true;
var _13=_1.delegate(this.kwArgs);
_13.onComplete=_1.hitch(this,"_onFetchComplete");
_13.onError=_1.hitch(this,"onFetchError");
this.store.fetch(_13);
}
},_onFetchComplete:function(_14,_15){
this.items=_14;
this._buildItemMap();
this.data=_3.map(this.items,function(_16){
return this.value(this.store,_16);
},this);
this._pushDataChanges();
this._inFlight=false;
},onFetchError:function(_17,_18){
this._inFlight=false;
},_buildItemMap:function(){
if(this.store.getFeatures()["dojo.data.api.Identity"]){
var _19={};
_3.forEach(this.items,function(_1a,_1b){
_19[this.store.getIdentity(_1a)]=_1b;
},this);
this.itemMap=_19;
}
},_pushDataChanges:function(){
if(this.series){
this.series.chart.updateSeries(this.series.name,this);
this.series.chart.delayedRender();
}
},_onStoreNew:function(){
this.fetch();
},_onStoreDelete:function(_1c){
if(this.items){
var _1d=_3.some(this.items,function(it,_1e){
if(it===_1c){
this.items.splice(_1e,1);
this._buildItemMap();
this.data.splice(_1e,1);
return true;
}
return false;
},this);
if(_1d){
this._pushDataChanges();
}
}
},_onStoreSet:function(_1f){
if(this.itemMap){
var id=this.store.getIdentity(_1f),_20=this.itemMap[id];
if(typeof _20=="number"){
this.data[_20]=this.value(this.store,this.items[_20]);
this._pushDataChanges();
}
}else{
if(this.items){
var _21=_3.some(this.items,function(it,_22){
if(it===_1f){
this.data[_22]=this.value(this.store,it);
return true;
}
return false;
},this);
if(_21){
this._pushDataChanges();
}
}
}
}});
});
