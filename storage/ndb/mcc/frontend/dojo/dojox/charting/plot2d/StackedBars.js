//>>built
define("dojox/charting/plot2d/StackedBars",["dojo/_base/declare","dojo/_base/lang","./Bars","./commonStacked"],function(_1,_2,_3,_4){
return _1("dojox.charting.plot2d.StackedBars",_3,{getSeriesStats:function(){
var _5=_4.collectStats(this.series,_2.hitch(this,"isNullValue")),t;
_5.hmin-=0.5;
_5.hmax+=0.5;
t=_5.hmin,_5.hmin=_5.vmin,_5.vmin=t;
t=_5.hmax,_5.hmax=_5.vmax,_5.vmax=t;
return _5;
},rearrangeValues:function(_6,_7,_8){
return _4.rearrangeValues.call(this,_6,_7,_8);
}});
});
