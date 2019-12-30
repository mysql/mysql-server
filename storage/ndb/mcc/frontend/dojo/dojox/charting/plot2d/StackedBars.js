//>>built
define("dojox/charting/plot2d/StackedBars",["dojo/_base/declare","./Bars","./commonStacked"],function(_1,_2,_3){
return _1("dojox.charting.plot2d.StackedBars",_2,{getSeriesStats:function(){
var _4=_3.collectStats(this.series),t;
_4.hmin-=0.5;
_4.hmax+=0.5;
t=_4.hmin,_4.hmin=_4.vmin,_4.vmin=t;
t=_4.hmax,_4.hmax=_4.vmax,_4.vmax=t;
return _4;
},getValue:function(_5,_6,_7,_8){
var y,x;
if(_8){
x=_6;
y=_3.getIndexValue(this.series,_7,x);
}else{
x=_5.x-1;
y=_3.getValue(this.series,_7,_5.x);
y=y?y.y:null;
}
return {y:y,x:x};
}});
});
