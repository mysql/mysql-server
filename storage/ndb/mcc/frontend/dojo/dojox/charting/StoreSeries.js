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
this.fetch();
},destroy:function(){
if(this.observeHandle){
this.observeHandle.dismiss();
}
},setSeriesObject:function(_b){
this.series=_b;
},fetch:function(){
var _c=this.objects=[];
var _d=this;
if(this.observeHandle){
this.observeHandle.dismiss();
}
var _e=this.store.query(this.kwArgs.query,this.kwArgs);
_3.when(_e,function(_f){
_d.objects=_f;
_10();
});
if(_e.observe){
this.observeHandle=_e.observe(_10,true);
}
function _10(){
_d.data=_1.map(_d.objects,function(_11){
return _d.value(_11,_d.store);
});
_d._pushDataChanges();
};
},_pushDataChanges:function(){
if(this.series){
this.series.chart.updateSeries(this.series.name,this);
this.series.chart.delayedRender();
}
}});
});
