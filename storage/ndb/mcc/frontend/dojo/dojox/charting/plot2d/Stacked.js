//>>built
define("dojox/charting/plot2d/Stacked",["dojo/_base/declare","dojo/_base/lang","./Default","./commonStacked"],function(_1,_2,_3,_4){
return _1("dojox.charting.plot2d.Stacked",_3,{getSeriesStats:function(){
var _5=_4.collectStats(this.series,_2.hitch(this,"isNullValue"));
return _5;
},buildSegments:function(i,_6){
var _7=this.series[i],_8=_6?Math.max(0,Math.floor(this._hScaler.bounds.from-1)):0,_9=_6?Math.min(_7.data.length-1,Math.ceil(this._hScaler.bounds.to)):_7.data.length-1,_a=null,_b=[],_c=_2.hitch(this,"isNullValue");
for(var j=_8;j<=_9;j++){
var _d=_6?_4.getIndexValue(this.series,i,j,_c):_4.getValue(this.series,i,_7.data[j]?_7.data[j].x:null,_c);
if(!_c(_d[0])&&(_6||_d[0].y!=null)){
if(!_a){
_a=[];
_b.push({index:j,rseg:_a});
}
_a.push(_d[0]);
}else{
if(!this.opt.interpolate||_6){
_a=null;
}
}
}
return _b;
}});
});
