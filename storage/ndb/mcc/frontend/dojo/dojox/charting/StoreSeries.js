//>>built
define("dojox/charting/StoreSeries",["dojo/_base/array","dojo/_base/declare","dojo/_base/Deferred"],function(_1,_2,_3){
return _2("dojox.charting.StoreSeries",null,{constructor:function(_4,_5,_6){
this.store=_4;
this.kwArgs=_5;
if(_6){
if(typeof _6=="function"){
this.value=_6;
}else{
if(typeof _6=="object"){
this.value=function(_7){
var o={};
for(var _8 in _6){
o[_8]=_7[_6[_8]];
}
return o;
};
}else{
this.value=function(_9){
return _9[_6];
};
}
}
}else{
this.value=function(_a){
return _a.value;
};
}
this.data=[];
this._initialRendering=true;
this.fetch();
},destroy:function(){
if(this.observeHandle){
this.observeHandle.remove();
}
},setSeriesObject:function(_b){
this.series=_b;
},fetch:function(_c,_d){
var _e=this;
if(this.observeHandle){
this.observeHandle.remove();
}
var _f=this.store.query(_c||this.kwArgs.query,_d||this.kwArgs);
_3.when(_f,function(_10){
_e.objects=_10;
_11();
});
if(_f.observe){
this.observeHandle=_f.observe(_11,true);
}
function _11(){
_e.data=_1.map(_e.objects,function(_12){
return _e.value(_12,_e.store);
});
_e._pushDataChanges();
};
},_pushDataChanges:function(){
if(this.series){
this.series.chart.updateSeries(this.series.name,this,this._initialRendering);
this._initialRendering=false;
this.series.chart.delayedRender();
}
}});
});
