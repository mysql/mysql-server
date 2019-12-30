//>>built
define("dojox/charting/plot2d/Stacked",["dojo/_base/declare","./Default","./commonStacked"],function(_1,_2,_3){
return _1("dojox.charting.plot2d.Stacked",_2,{getSeriesStats:function(){
var _4=_3.collectStats(this.series);
return _4;
},buildSegments:function(i,_5){
var _6=this.series[i],_7=_5?Math.max(0,Math.floor(this._hScaler.bounds.from-1)):0,_8=_5?Math.min(_6.data.length-1,Math.ceil(this._hScaler.bounds.to)):_6.data.length-1,_9=null,_a=[];
for(var j=_7;j<=_8;j++){
var _b=_5?_3.getIndexValue(this.series,i,j):_3.getValue(this.series,i,_6.data[j]?_6.data[j].x:null);
if(_b!=null&&(_5||_b.y!=null)){
if(!_9){
_9=[];
_a.push({index:j,rseg:_9});
}
_9.push(_b);
}else{
if(!this.opt.interpolate||_5){
_9=null;
}
}
}
return _a;
}});
});
