//>>built
define("dojox/charting/themes/Electric",["../Theme","dojox/gfx/gradutils","./common"],function(_1,_2,_3){
var g=_1.generateGradient,_4={type:"linear",space:"shape",x1:0,y1:0,x2:0,y2:75};
_3.Electric=new _1({chart:{fill:"#252525",stroke:{color:"#252525"},pageStyle:{backgroundColor:"#252525",backgroundImage:"none",color:"#ccc"}},plotarea:{fill:"#252525"},axis:{stroke:{color:"#aaa",width:1},tick:{color:"#777",position:"center",font:"normal normal normal 7pt Helvetica, Arial, sans-serif",fontColor:"#777"}},series:{stroke:{width:2,color:"#ccc"},outline:null,font:"normal normal normal 8pt Helvetica, Arial, sans-serif",fontColor:"#ccc"},marker:{stroke:{width:3,color:"#ccc"},outline:null,font:"normal normal normal 8pt Helvetica, Arial, sans-serif",fontColor:"#ccc"},seriesThemes:[{fill:g(_4,"#004cbf","#06f")},{fill:g(_4,"#bf004c","#f06")},{fill:g(_4,"#43bf00","#6f0")},{fill:g(_4,"#7300bf","#90f")},{fill:g(_4,"#bf7300","#f90")},{fill:g(_4,"#00bf73","#0f9")}],markerThemes:[{fill:"#06f",stroke:{color:"#06f"}},{fill:"#f06",stroke:{color:"#f06"}},{fill:"#6f0",stroke:{color:"#6f0"}},{fill:"#90f",stroke:{color:"#90f"}},{fill:"#f90",stroke:{color:"#f90"}},{fill:"#0f9",stroke:{color:"#0f9"}}]});
_3.Electric.next=function(_5,_6,_7){
var _8=_5=="line";
if(_8||_5=="area"){
var s=this.seriesThemes[this._current%this.seriesThemes.length];
s.fill.space="plot";
if(_8){
s.stroke={width:2.5,color:s.fill.colors[1].color};
}
if(_5=="area"){
s.fill.y2=90;
}
var _9=_1.prototype.next.apply(this,arguments);
delete s.stroke;
s.fill.y2=75;
s.fill.space="shape";
return _9;
}
return _1.prototype.next.apply(this,arguments);
};
_3.Electric.post=function(_a,_b){
_a=_1.prototype.post.apply(this,arguments);
if((_b=="slice"||_b=="circle")&&_a.series.fill&&_a.series.fill.type=="radial"){
_a.series.fill=_2.reverse(_a.series.fill);
}
return _a;
};
return _3.Electric;
});
