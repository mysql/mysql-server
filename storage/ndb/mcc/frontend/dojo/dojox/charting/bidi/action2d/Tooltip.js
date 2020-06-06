//>>built
define("dojox/charting/bidi/action2d/Tooltip",["dojo/_base/declare","dojo/dom-style"],function(_1,_2){
return _1(null,{_recheckPosition:function(_3,_4,_5){
if(!this.chart.isRightToLeft()){
return;
}
var _6=this.chart.offsets.l-this.chart.offsets.r;
if(_3.element=="marker"){
_4.x=this.chart.dim.width-_3.cx+_6;
_5[0]="before-centered";
_5[1]="after-centered";
}else{
if(_3.element=="circle"){
_4.x=this.chart.dim.width-_3.cx-_3.cr+_6;
}else{
if(_3.element=="bar"||_3.element=="column"){
_4.x=this.chart.dim.width-_4.width-_4.x+_6;
if(_3.element=="bar"){
_5[0]="before-centered";
_5[1]="after-centered";
}
}else{
if(_3.element=="candlestick"){
_4.x=this.chart.dim.width+_6-_3.x;
}else{
if(_3.element=="slice"){
if((_5[0]=="before-centered")||(_5[0]=="after-centered")){
_5.reverse();
}
_4.x=_3.cx+(_3.cx-_4.x);
}
}
}
}
}
},_format:function(_7){
var _8=(_2.get(this.chart.node,"direction")=="rtl");
var _9=(this.chart.getTextDir(_7)=="rtl");
if(_9&&!_8){
return "<span dir = 'rtl'>"+_7+"</span>";
}else{
if(!_9&&_8){
return "<span dir = 'ltr'>"+_7+"</span>";
}else{
return _7;
}
}
}});
});
