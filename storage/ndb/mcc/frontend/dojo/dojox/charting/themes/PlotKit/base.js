//>>built
define("dojox/charting/themes/PlotKit/base",["dojo/_base/lang","dojo/_base/Color","../../Theme","../common"],function(_1,_2,_3,_4){
var pk=_1.getObject("PlotKit",true,_4);
pk.base=new _3({chart:{stroke:null,fill:"yellow"},plotarea:{stroke:null,fill:"yellow"},axis:{stroke:{color:"#fff",width:1},line:{color:"#fff",width:0.5},majorTick:{color:"#fff",width:0.5,length:6},minorTick:{color:"#fff",width:0.5,length:3},tick:{font:"normal normal normal 7pt Helvetica,Arial,sans-serif",fontColor:"#999"}},series:{stroke:{width:2.5,color:"#fff"},fill:"#666",font:"normal normal normal 7.5pt Helvetica,Arial,sans-serif",fontColor:"#666"},marker:{stroke:{width:2},fill:"#333",font:"normal normal normal 7pt Helvetica,Arial,sans-serif",fontColor:"#666"},colors:["red","green","blue"]});
pk.base.next=function(_5,_6,_7){
var _8=_3.prototype.next.apply(this,arguments);
if(_5=="line"){
_8.marker.outline={width:2,color:"#fff"};
_8.series.stroke.width=3.5;
_8.marker.stroke.width=2;
}else{
if(_5=="candlestick"){
_8.series.stroke.width=1;
}else{
if(_8.series.stroke.color&&(_8.series.stroke.color.toString()==new _2(this.colors[(this._current-1)%this.colors.length]).toString())){
_8.series.stroke.color="#fff";
}
}
}
return _8;
};
return pk;
});
