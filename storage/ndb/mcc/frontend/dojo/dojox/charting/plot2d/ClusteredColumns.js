//>>built
define("dojox/charting/plot2d/ClusteredColumns",["dojo/_base/declare","dojo/_base/array","./Columns","./common"],function(_1,_2,_3,dc){
return _1("dojox.charting.plot2d.ClusteredColumns",_3,{getBarProperties:function(){
var _4=this.series.length;
_2.forEach(this.series,function(_5){
if(_5.hidden){
_4--;
}
});
var f=dc.calculateBarSize(this._hScaler.bounds.scale,this.opt,_4);
return {gap:f.gap,width:f.size,thickness:f.size,clusterSize:_4};
}});
});
