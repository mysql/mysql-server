//>>built
define("dojox/charting/plot2d/ClusteredBars",["dojo/_base/declare","dojo/_base/array","./Bars","./common"],function(_1,_2,_3,dc){
return _1("dojox.charting.plot2d.ClusteredBars",_3,{getBarProperties:function(){
var _4=this.series.length;
_2.forEach(this.series,function(_5){
if(_5.hidden){
_4--;
}
});
var f=dc.calculateBarSize(this._vScaler.bounds.scale,this.opt,_4);
return {gap:f.gap,height:f.size,thickness:f.size};
}});
});
