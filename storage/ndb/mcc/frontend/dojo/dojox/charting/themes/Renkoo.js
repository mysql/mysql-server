//>>built
define("dojox/charting/themes/Renkoo",["../Theme","dojox/gfx/gradutils","./common"],function(_1,_2,_3){
var g=_1.generateGradient,_4={type:"linear",space:"shape",x1:0,y1:0,x2:0,y2:150};
_3.Renkoo=new _1({chart:{fill:"#123666",pageStyle:{backgroundColor:"#123666",backgroundImage:"none",color:"#95afdb"}},plotarea:{fill:"#123666"},axis:{stroke:{color:"#95afdb",width:1},tick:{color:"#95afdb",position:"center",font:"normal normal normal 7pt Lucida Grande, Helvetica, Arial, sans-serif",fontColor:"#95afdb"}},series:{stroke:{width:2.5,color:"#123666"},outline:null,font:"normal normal normal 8pt Lucida Grande, Helvetica, Arial, sans-serif",fontColor:"#95afdb"},marker:{stroke:{width:2.5,color:"#ccc"},outline:null,font:"normal normal normal 8pt Lucida Grande, Helvetica, Arial, sans-serif",fontColor:"#95afdb"},seriesThemes:[{fill:g(_4,"#e7e391","#f8f7de")},{fill:g(_4,"#ffb6b6","#ffe8e8")},{fill:g(_4,"#bcda7d","#eef7da")},{fill:g(_4,"#d5d5d5","#f4f4f4")},{fill:g(_4,"#c1e3fd","#e4f3ff")}],markerThemes:[{fill:"#fcfcf3",stroke:{color:"#e7e391"}},{fill:"#fff1f1",stroke:{color:"#ffb6b6"}},{fill:"#fafdf4",stroke:{color:"#bcda7d"}},{fill:"#fbfbfb",stroke:{color:"#d5d5d5"}},{fill:"#f3faff",stroke:{color:"#c1e3fd"}}]});
_3.Renkoo.next=function(_5,_6,_7){
if("slice,column,bar".indexOf(_5)==-1){
var s=this.seriesThemes[this._current%this.seriesThemes.length];
s.fill.space="plot";
s.stroke={width:2,color:s.fill.colors[0].color};
if(_5=="line"||_5=="area"){
s.stroke.width=4;
}
var _8=_1.prototype.next.apply(this,arguments);
delete s.stroke;
s.fill.space="shape";
return _8;
}
return _1.prototype.next.apply(this,arguments);
};
_3.Renkoo.post=function(_9,_a){
_9=_1.prototype.post.apply(this,arguments);
if((_a=="slice"||_a=="circle")&&_9.series.fill&&_9.series.fill.type=="radial"){
_9.series.fill=_2.reverse(_9.series.fill);
}
return _9;
};
return _3.Renkoo;
});
