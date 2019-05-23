//>>built
define("dojox/charting/action2d/Tooltip",["dojo/_base/kernel","dijit/Tooltip","dojo/_base/lang","dojo/_base/declare","dojo/dom-style","./PlotAction","dojox/gfx/matrix","dojox/lang/functional","dojox/lang/functional/scan","dojox/lang/functional/fold"],function(_1,_2,_3,_4,_5,_6,m,df,_7,_8){
var _9=function(o){
var t=o.run&&o.run.data&&o.run.data[o.index];
if(t&&typeof t!="number"&&(t.tooltip||t.text)){
return t.tooltip||t.text;
}
if(o.element=="candlestick"){
return "<table cellpadding=\"1\" cellspacing=\"0\" border=\"0\" style=\"font-size:0.9em;\">"+"<tr><td>Open:</td><td align=\"right\"><strong>"+o.data.open+"</strong></td></tr>"+"<tr><td>High:</td><td align=\"right\"><strong>"+o.data.high+"</strong></td></tr>"+"<tr><td>Low:</td><td align=\"right\"><strong>"+o.data.low+"</strong></td></tr>"+"<tr><td>Close:</td><td align=\"right\"><strong>"+o.data.close+"</strong></td></tr>"+(o.data.mid!==undefined?"<tr><td>Mid:</td><td align=\"right\"><strong>"+o.data.mid+"</strong></td></tr>":"")+"</table>";
}
return o.element=="bar"?o.x:o.y;
};
var _a=Math.PI/4,_b=Math.PI/2;
return _4("dojox.charting.action2d.Tooltip",_6,{defaultParams:{text:_9},optionalParams:{},constructor:function(_c,_d,_e){
this.text=_e&&_e.text?_e.text:_9;
this.connect();
},process:function(o){
if(o.type==="onplotreset"||o.type==="onmouseout"){
_2.hide(this.aroundRect);
this.aroundRect=null;
if(o.type==="onplotreset"){
delete this.angles;
}
return;
}
if(!o.shape||o.type!=="onmouseover"){
return;
}
var _f={type:"rect"},_10=["after-centered","before-centered"];
switch(o.element){
case "marker":
_f.x=o.cx;
_f.y=o.cy;
_f.w=_f.h=1;
break;
case "circle":
_f.x=o.cx-o.cr;
_f.y=o.cy-o.cr;
_f.w=_f.h=2*o.cr;
break;
case "spider_circle":
_f.x=o.cx;
_f.y=o.cy;
_f.w=_f.h=1;
break;
case "spider_plot":
return;
case "column":
_10=["above-centered","below-centered"];
case "bar":
_f=_3.clone(o.shape.getShape());
_f.w=_f.width;
_f.h=_f.height;
break;
case "candlestick":
_f.x=o.x;
_f.y=o.y;
_f.w=o.width;
_f.h=o.height;
break;
default:
if(!this.angles){
if(typeof o.run.data[0]=="number"){
this.angles=df.map(df.scanl(o.run.data,"+",0),"* 2 * Math.PI / this",df.foldl(o.run.data,"+",0));
}else{
this.angles=df.map(df.scanl(o.run.data,"a + b.y",0),"* 2 * Math.PI / this",df.foldl(o.run.data,"a + b.y",0));
}
}
var _11=m._degToRad(o.plot.opt.startAngle),_12=(this.angles[o.index]+this.angles[o.index+1])/2+_11;
_f.x=o.cx+o.cr*Math.cos(_12);
_f.y=o.cy+o.cr*Math.sin(_12);
_f.w=_f.h=1;
if(_11&&(_12<0||_12>2*Math.PI)){
_12=Math.abs(2*Math.PI-Math.abs(_12));
}
if(_12<_a){
}else{
if(_12<_b+_a){
_10=["below-centered","above-centered"];
}else{
if(_12<Math.PI+_a){
_10=["before-centered","after-centered"];
}else{
if(_12<2*Math.PI-_a){
_10=["above-centered","below-centered"];
}
}
}
}
break;
}
var lt=this.chart.getCoords();
_f.x+=lt.x;
_f.y+=lt.y;
_f.x=Math.round(_f.x);
_f.y=Math.round(_f.y);
_f.w=Math.ceil(_f.w);
_f.h=Math.ceil(_f.h);
this.aroundRect=_f;
var _13=this.text(o);
if(this.chart.getTextDir){
var _14=(_5.get(this.chart.node,"direction")=="rtl");
var _15=(this.chart.getTextDir(_13)=="rtl");
}
if(_13){
if(_15&&!_14){
_2.show("<span dir = 'rtl'>"+_13+"</span>",this.aroundRect,_10);
}else{
if(!_15&&_14){
_2.show("<span dir = 'ltr'>"+_13+"</span>",this.aroundRect,_10);
}else{
_2.show(_13,this.aroundRect,_10);
}
}
}
}});
});
