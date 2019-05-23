//>>built
define("dojox/charting/plot2d/ClusteredColumns",["dojo/_base/declare","./Columns","./common"],function(_1,_2,dc){
return _1("dojox.charting.plot2d.ClusteredColumns",_2,{getBarProperties:function(){
var f=dc.calculateBarSize(this._hScaler.bounds.scale,this.opt,this.series.length);
return {gap:f.gap,width:f.size,thickness:f.size};
}});
});
