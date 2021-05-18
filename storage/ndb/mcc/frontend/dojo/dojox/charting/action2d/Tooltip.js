//>>built
define("dojox/charting/action2d/Tooltip",["dijit/Tooltip","dojo/_base/lang","dojo/_base/declare","dojo/_base/window","dojo/_base/connect","dojo/dom-style","./PlotAction","dojox/gfx/matrix","dojo/has","dojo/has!dojo-bidi?../bidi/action2d/Tooltip","dojox/lang/functional","dojox/lang/functional/scan","dojox/lang/functional/fold"],function(_1,_2,_3,_4,_5,_6,_7,m,_8,_9,df){
var _a=function(o,_b){
var t=o.run&&o.run.data&&o.run.data[o.index];
if(t&&typeof t!="number"&&(t.tooltip||t.text)){
return t.tooltip||t.text;
}
if(_b.tooltipFunc){
return _b.tooltipFunc(o);
}else{
return o.y;
}
};
var _c=Math.PI/4,_d=Math.PI/2;
var _e=_3(_8("dojo-bidi")?"dojox.charting.action2d.NonBidiTooltip":"dojox.charting.action2d.Tooltip",_7,{defaultParams:{text:_a,mouseOver:true,defaultPosition:["after-centered","before-centered"]},optionalParams:{},constructor:function(_f,_10,_11){
this.text=_11&&_11.text?_11.text:_a;
this.mouseOver=_11&&_11.mouseOver!=undefined?_11.mouseOver:true;
this.defaultPosition=_11&&_11.defaultPosition!=undefined?_11.defaultPosition:["after-centered","before-centered"];
this.connect();
},process:function(o){
if(o.type==="onplotreset"||o.type==="onmouseout"){
_1.hide(this.aroundRect);
this.aroundRect=null;
if(o.type==="onplotreset"){
delete this.angles;
}
return;
}
if(!o.shape||(this.mouseOver&&o.type!=="onmouseover")||(!this.mouseOver&&o.type!=="onclick")){
return;
}
var _12={type:"rect"},_13=this.defaultPosition;
switch(o.element){
case "marker":
_12.x=o.cx;
_12.y=o.cy;
_12.w=_12.h=1;
break;
case "circle":
_12.x=o.cx-o.cr;
_12.y=o.cy-o.cr;
_12.w=_12.h=2*o.cr;
break;
case "spider_circle":
_12.x=o.cx;
_12.y=o.cy;
_12.w=_12.h=1;
break;
case "spider_plot":
return;
case "column":
_13=["above-centered","below-centered"];
case "bar":
_12=_2.clone(o.shape.getShape());
_12.w=_12.width;
_12.h=_12.height;
break;
case "candlestick":
_12.x=o.x;
_12.y=o.y;
_12.w=o.width;
_12.h=o.height;
break;
default:
if(!this.angles){
var _14=typeof o.run.data[0]=="number"?df.map(o.run.data,"x ? Math.max(x, 0) : 0"):df.map(o.run.data,"x ? Math.max(x.y, 0) : 0");
this.angles=df.map(df.scanl(_14,"+",0),"* 2 * Math.PI / this",df.foldl(_14,"+",0));
}
var _15=m._degToRad(o.plot.opt.startAngle),_16=(this.angles[o.index]+this.angles[o.index+1])/2+_15;
_12.x=o.cx+o.cr*Math.cos(_16);
_12.y=o.cy+o.cr*Math.sin(_16);
_12.w=_12.h=1;
if(_15&&(_16<0||_16>2*Math.PI)){
_16=Math.abs(2*Math.PI-Math.abs(_16));
}
if(_16<_c){
}else{
if(_16<_d+_c){
_13=["below-centered","above-centered"];
}else{
if(_16<Math.PI+_c){
_13=["before-centered","after-centered"];
}else{
if(_16<2*Math.PI-_c){
_13=["above-centered","below-centered"];
}
}
}
}
break;
}
if(_8("dojo-bidi")){
this._recheckPosition(o,_12,_13);
}
var lt=this.chart.getCoords();
_12.x+=lt.x;
_12.y+=lt.y;
_12.x=Math.round(_12.x);
_12.y=Math.round(_12.y);
_12.w=Math.ceil(_12.w);
_12.h=Math.ceil(_12.h);
this.aroundRect=_12;
var _17=this.text(o,this.plot);
if(_17){
_1.show(this._format(_17),this.aroundRect,_13);
}
if(!this.mouseOver){
this._handle=_5.connect(_4.doc,"onclick",this,"onClick");
}
},onClick:function(){
this.process({type:"onmouseout"});
},_recheckPosition:function(obj,_18,_19){
},_format:function(_1a){
return _1a;
}});
return _8("dojo-bidi")?_3("dojox.charting.action2d.Tooltip",[_e,_9]):_e;
});
