//>>built
define("dojox/charting/plot2d/Base",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojox/gfx","../Element","./common","../axis2d/common","dojo/has"],function(_1,_2,_3,_4,_5,_6,ac,_7){
var _8=_1("dojox.charting.plot2d.Base",_5,{constructor:function(_9,_a){
if(_a&&_a.tooltipFunc){
this.tooltipFunc=_a.tooltipFunc;
}
},clear:function(){
this.series=[];
this.dirty=true;
return this;
},setAxis:function(_b){
return this;
},assignAxes:function(_c){
_2.forEach(this.axes,function(_d){
if(this[_d]){
this.setAxis(_c[this[_d]]);
}
},this);
},addSeries:function(_e){
this.series.push(_e);
return this;
},getSeriesStats:function(){
return _6.collectSimpleStats(this.series,_3.hitch(this,"isNullValue"));
},calculateAxes:function(_f){
this.initializeScalers(_f,this.getSeriesStats());
return this;
},initializeScalers:function(){
return this;
},isDataDirty:function(){
return _2.some(this.series,function(_10){
return _10.dirty;
});
},render:function(dim,_11){
return this;
},renderLabel:function(_12,x,y,_13,_14,_15,_16){
var _17=ac.createText[this.opt.htmlLabels&&_4.renderer!="vml"?"html":"gfx"](this.chart,_12,x,y,_16?_16:"middle",_13,_14.series.font,_14.series.fontColor);
if(_15){
if(this.opt.htmlLabels&&_4.renderer!="vml"){
_17.style.pointerEvents="none";
}else{
if(_17.rawNode){
_17.rawNode.style.pointerEvents="none";
}
}
}
if(this.opt.htmlLabels&&_4.renderer!="vml"){
this.htmlElements.push(_17);
}
return _17;
},getRequiredColors:function(){
return this.series.length;
},_getLabel:function(_18){
return _6.getLabel(_18,this.opt.fixed,this.opt.precision);
}});
if(_7("dojo-bidi")){
_8.extend({_checkOrientation:function(_19,dim,_1a){
this.chart.applyMirroring(this.group,dim,_1a);
}});
}
return _8;
});
