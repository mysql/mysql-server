//>>built
define("dojox/charting/themes/Tom",["../Theme","dojox/gfx/gradutils","./common"],function(_1,_2,_3){
var g=_1.generateGradient,_4={type:"linear",space:"shape",x1:0,y1:0,x2:0,y2:100};
_3.Tom=new _1({chart:{fill:"#181818",stroke:{color:"#181818"},pageStyle:{backgroundColor:"#181818",backgroundImage:"none",color:"#eaf2cb"}},plotarea:{fill:"#181818"},axis:{stroke:{color:"#a0a68b",width:1},tick:{color:"#888c76",position:"center",font:"normal normal normal 7pt Helvetica, Arial, sans-serif",fontColor:"#888c76"}},series:{stroke:{width:2.5,color:"#eaf2cb"},outline:null,font:"normal normal normal 8pt Helvetica, Arial, sans-serif",fontColor:"#eaf2cb"},marker:{stroke:{width:1.25,color:"#eaf2cb"},outline:{width:1.25,color:"#eaf2cb"},font:"normal normal normal 8pt Helvetica, Arial, sans-serif",fontColor:"#eaf2cb"},seriesThemes:[{fill:g(_4,"#bf9e0a","#ecc20c")},{fill:g(_4,"#73b086","#95e5af")},{fill:g(_4,"#c7212d","#ed2835")},{fill:g(_4,"#87ab41","#b6e557")},{fill:g(_4,"#b86c25","#d37d2a")}],markerThemes:[{fill:"#bf9e0a",stroke:{color:"#ecc20c"}},{fill:"#73b086",stroke:{color:"#95e5af"}},{fill:"#c7212d",stroke:{color:"#ed2835"}},{fill:"#87ab41",stroke:{color:"#b6e557"}},{fill:"#b86c25",stroke:{color:"#d37d2a"}}]});
_3.Tom.next=function(_5,_6,_7){
var _8=_5=="line";
if(_8||_5=="area"){
var s=this.seriesThemes[this._current%this.seriesThemes.length];
s.fill.space="plot";
if(_8){
s.stroke={width:4,color:s.fill.colors[0].color};
}
var _9=_1.prototype.next.apply(this,arguments);
delete s.outline;
delete s.stroke;
s.fill.space="shape";
return _9;
}
return _1.prototype.next.apply(this,arguments);
};
_3.Tom.post=function(_a,_b){
_a=_1.prototype.post.apply(this,arguments);
if((_b=="slice"||_b=="circle")&&_a.series.fill&&_a.series.fill.type=="radial"){
_a.series.fill=_2.reverse(_a.series.fill);
}
return _a;
};
return _3.Tom;
});
