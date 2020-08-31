//>>built
define("dojox/charting/themes/Chris",["../Theme","dojox/gfx/gradutils","./common"],function(_1,_2,_3){
var g=_1.generateGradient,_4={type:"linear",space:"shape",x1:0,y1:0,x2:0,y2:100};
_3.Chris=new _1({chart:{fill:"#c1c1c1",stroke:{color:"#666"}},plotarea:{fill:"#c1c1c1"},series:{stroke:{width:2,color:"white"},outline:null,fontColor:"#333"},marker:{stroke:{width:2,color:"white"},outline:{width:2,color:"white"},fontColor:"#333"},seriesThemes:[{fill:g(_4,"#01b717","#238c01")},{fill:g(_4,"#d04918","#7c0344")},{fill:g(_4,"#0005ec","#002578")},{fill:g(_4,"#f9e500","#786f00")},{fill:g(_4,"#e27d00","#773e00")},{fill:g(_4,"#00b5b0","#005f5d")},{fill:g(_4,"#ac00cb","#590060")}],markerThemes:[{fill:"#01b717",stroke:{color:"#238c01"}},{fill:"#d04918",stroke:{color:"#7c0344"}},{fill:"#0005ec",stroke:{color:"#002578"}},{fill:"#f9e500",stroke:{color:"#786f00"}},{fill:"#e27d00",stroke:{color:"#773e00"}},{fill:"#00b5b0",stroke:{color:"#005f5d"}},{fill:"#ac00cb",stroke:{color:"#590060"}}]});
_3.Chris.next=function(_5,_6,_7){
var _8=_5=="line";
if(_8||_5=="area"){
var s=this.seriesThemes[this._current%this.seriesThemes.length];
s.fill.space="plot";
if(_8){
s.stroke={color:s.fill.colors[1].color};
s.outline={width:2,color:"white"};
}
var _9=_1.prototype.next.apply(this,arguments);
delete s.outline;
delete s.stroke;
s.fill.space="shape";
return _9;
}
return _1.prototype.next.apply(this,arguments);
};
_3.Chris.post=function(_a,_b){
_a=_1.prototype.post.apply(this,arguments);
if((_b=="slice"||_b=="circle")&&_a.series.fill&&_a.series.fill.type=="radial"){
_a.series.fill=_2.reverse(_a.series.fill);
}
return _a;
};
return _3.Chris;
});
