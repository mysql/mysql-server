//>>built
define("dojox/charting/plot2d/ClusteredBars",["dojo/_base/declare","./Bars","./common"],function(_1,_2,dc){
return _1("dojox.charting.plot2d.ClusteredBars",_2,{getBarProperties:function(){
var f=dc.calculateBarSize(this._vScaler.bounds.scale,this.opt,this.series.length);
return {gap:f.gap,height:f.size,thickness:f.size};
}});
});
