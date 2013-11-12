//>>built
define("dojox/charting/axis2d/Invisible",["dojo/_base/lang","dojo/_base/declare","./Base","../scaler/linear","dojox/gfx","dojox/lang/utils","dojox/lang/functional","dojo/string"],function(_1,_2,_3,_4,g,du,df,_5){
var _6=du.merge,_7=4,_8=45;
return _2("dojox.charting.axis2d.Invisible",_3,{defaultParams:{vertical:false,fixUpper:"none",fixLower:"none",natural:false,leftBottom:true,includeZero:false,fixed:true,majorLabels:true,minorTicks:true,minorLabels:true,microTicks:false,rotation:0},optionalParams:{min:0,max:1,from:0,to:1,majorTickStep:4,minorTickStep:2,microTickStep:1,labels:[],labelFunc:null,maxLabelSize:0,maxLabelCharCount:0,trailingSymbol:null},constructor:function(_9,_a){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_a);
du.updateWithPattern(this.opt,_a,this.optionalParams);
},dependOnData:function(){
return !("min" in this.opt)||!("max" in this.opt);
},clear:function(){
delete this.scaler;
delete this.ticks;
this.dirty=true;
return this;
},initialized:function(){
return "scaler" in this&&!(this.dirty&&this.dependOnData());
},setWindow:function(_b,_c){
this.scale=_b;
this.offset=_c;
return this.clear();
},getWindowScale:function(){
return "scale" in this?this.scale:1;
},getWindowOffset:function(){
return "offset" in this?this.offset:0;
},_groupLabelWidth:function(_d,_e,_f){
if(!_d.length){
return 0;
}
if(_1.isObject(_d[0])){
_d=df.map(_d,function(_10){
return _10.text;
});
}
if(_f){
_d=df.map(_d,function(_11){
return _1.trim(_11).length==0?"":_11.substring(0,_f)+this.trailingSymbol;
},this);
}
var s=_d.join("<br>");
return g._base._getTextBox(s,{font:_e}).w||0;
},calculate:function(min,max,_12,_13){
if(this.initialized()){
return this;
}
var o=this.opt;
this.labels="labels" in o?o.labels:_13;
this.scaler=_4.buildScaler(min,max,_12,o);
var tsb=this.scaler.bounds;
if("scale" in this){
o.from=tsb.lower+this.offset;
o.to=(tsb.upper-tsb.lower)/this.scale+o.from;
if(!isFinite(o.from)||isNaN(o.from)||!isFinite(o.to)||isNaN(o.to)||o.to-o.from>=tsb.upper-tsb.lower){
delete o.from;
delete o.to;
delete this.scale;
delete this.offset;
}else{
if(o.from<tsb.lower){
o.to+=tsb.lower-o.from;
o.from=tsb.lower;
}else{
if(o.to>tsb.upper){
o.from+=tsb.upper-o.to;
o.to=tsb.upper;
}
}
this.offset=o.from-tsb.lower;
}
this.scaler=_4.buildScaler(min,max,_12,o);
tsb=this.scaler.bounds;
if(this.scale==1&&this.offset==0){
delete this.scale;
delete this.offset;
}
}
var ta=this.chart.theme.axis,_14=0,_15=o.rotation%360,_16=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font),_17=_16?g.normalizedLength(g.splitFontString(_16).size):0,_18=Math.abs(Math.cos(_15*Math.PI/180)),_19=Math.abs(Math.sin(_15*Math.PI/180));
if(_15<0){
_15+=360;
}
if(_17){
if(this.vertical?_15!=0&&_15!=180:_15!=90&&_15!=270){
if(this.labels){
_14=this._groupLabelWidth(this.labels,_16,o.maxLabelCharCount);
}else{
var _1a=Math.ceil(Math.log(Math.max(Math.abs(tsb.from),Math.abs(tsb.to)))/Math.LN10),t=[];
if(tsb.from<0||tsb.to<0){
t.push("-");
}
t.push(_5.rep("9",_1a));
var _1b=Math.floor(Math.log(tsb.to-tsb.from)/Math.LN10);
if(_1b>0){
t.push(".");
t.push(_5.rep("9",_1b));
}
_14=g._base._getTextBox(t.join(""),{font:_16}).w;
}
_14=o.maxLabelSize?Math.min(o.maxLabelSize,_14):_14;
}else{
_14=_17;
}
switch(_15){
case 0:
case 90:
case 180:
case 270:
break;
default:
var _1c=Math.sqrt(_14*_14+_17*_17),_1d=this.vertical?_17*_18+_14*_19:_14*_18+_17*_19;
_14=Math.min(_1c,_1d);
break;
}
}
this.scaler.minMinorStep=_14+_7;
this.ticks=_4.buildTicks(this.scaler,o);
return this;
},getScaler:function(){
return this.scaler;
},getTicks:function(){
return this.ticks;
}});
});
