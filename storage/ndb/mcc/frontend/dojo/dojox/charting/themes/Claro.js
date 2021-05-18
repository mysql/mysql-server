//>>built
define("dojox/charting/themes/Claro",["../Theme","dojox/gfx/gradutils","./common"],function(_1,_2,_3){
var g=_1.generateGradient,_4={type:"linear",space:"shape",x1:0,y1:0,x2:0,y2:100};
_3.Claro=new _1({chart:{fill:{type:"linear",x1:0,x2:0,y1:0,y2:100,colors:[{offset:0,color:"#dbdbdb"},{offset:1,color:"#efefef"}]},stroke:{color:"#b5bcc7"}},plotarea:{fill:{type:"linear",x1:0,x2:0,y1:0,y2:100,colors:[{offset:0,color:"#dbdbdb"},{offset:1,color:"#efefef"}]}},axis:{stroke:{color:"#888c76",width:1},tick:{color:"#888c76",position:"center",font:"normal normal normal 7pt Verdana, Arial, sans-serif",fontColor:"#888c76"}},series:{stroke:{width:2.5,color:"#fff"},outline:null,font:"normal normal normal 7pt Verdana, Arial, sans-serif",fontColor:"#131313"},marker:{stroke:{width:1.25,color:"#131313"},outline:{width:1.25,color:"#131313"},font:"normal normal normal 8pt Verdana, Arial, sans-serif",fontColor:"#131313"},seriesThemes:[{fill:g(_4,"#2a6ead","#3a99f2")},{fill:g(_4,"#613e04","#996106")},{fill:g(_4,"#0e3961","#155896")},{fill:g(_4,"#55aafa","#3f7fba")},{fill:g(_4,"#ad7b2a","#db9b35")}],markerThemes:[{fill:"#2a6ead",stroke:{color:"#fff"}},{fill:"#613e04",stroke:{color:"#fff"}},{fill:"#0e3961",stroke:{color:"#fff"}},{fill:"#55aafa",stroke:{color:"#fff"}},{fill:"#ad7b2a",stroke:{color:"#fff"}}]});
_3.Claro.next=function(_5,_6,_7){
var _8=_5=="line",s,_9;
if(_8||_5=="area"){
s=this.seriesThemes[this._current%this.seriesThemes.length];
var m=this.markerThemes[this._current%this.markerThemes.length];
s.fill.space="plot";
if(_8){
s.stroke={width:4,color:s.fill.colors[0].color};
}
m.outline={width:1.25,color:m.fill};
_9=_1.prototype.next.apply(this,arguments);
delete s.outline;
delete s.stroke;
s.fill.space="shape";
return _9;
}else{
if(_5=="candlestick"){
s=this.seriesThemes[this._current%this.seriesThemes.length];
s.fill.space="plot";
s.stroke={width:1,color:s.fill.colors[0].color};
_9=_1.prototype.next.apply(this,arguments);
return _9;
}
}
return _1.prototype.next.apply(this,arguments);
};
_3.Claro.post=function(_a,_b){
_a=_1.prototype.post.apply(this,arguments);
if((_b=="slice"||_b=="circle")&&_a.series.fill&&_a.series.fill.type=="radial"){
_a.series.fill=_2.reverse(_a.series.fill);
}
return _a;
};
return _3.Claro;
});
