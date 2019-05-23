//>>built
define("dojox/charting/plot2d/Base",["dojo/_base/declare","../Element","dojo/_base/array","./common"],function(_1,_2,_3,_4){
return _1("dojox.charting.plot2d.Base",_2,{constructor:function(_5,_6){
},clear:function(){
this.series=[];
this.dirty=true;
return this;
},setAxis:function(_7){
return this;
},assignAxes:function(_8){
_3.forEach(this.axes,function(_9){
if(this[_9]){
this.setAxis(_8[this[_9]]);
}
},this);
},addSeries:function(_a){
this.series.push(_a);
return this;
},getSeriesStats:function(){
return _4.collectSimpleStats(this.series);
},calculateAxes:function(_b){
this.initializeScalers(_b,this.getSeriesStats());
return this;
},initializeScalers:function(){
return this;
},isDataDirty:function(){
return _3.some(this.series,function(_c){
return _c.dirty;
});
},render:function(_d,_e){
return this;
},getRequiredColors:function(){
return this.series.length;
}});
});
