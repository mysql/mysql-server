//>>built
define("dojox/charting/plot2d/StackedColumns",["dojo/_base/declare","dojo/_base/lang","./Columns","./commonStacked"],function(_1,_2,_3,_4){
return _1("dojox.charting.plot2d.StackedColumns",_3,{getSeriesStats:function(){
var _5=_4.collectStats(this.series,_2.hitch(this,"isNullValue"));
_5.hmin-=0.5;
_5.hmax+=0.5;
return _5;
},rearrangeValues:function(_6,_7,_8){
return _4.rearrangeValues.call(this,_6,_7,_8);
}});
});
