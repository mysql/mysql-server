//>>built
define("dojox/charting/Element",["dojo/_base/lang","dojo/_base/array","dojo/dom-construct","dojo/_base/declare","dojox/gfx"],function(_1,_2,_3,_4,_5){
return _4("dojox.charting.Element",null,{chart:null,group:null,htmlElements:null,dirty:true,constructor:function(_6){
this.chart=_6;
this.group=null;
this.htmlElements=[];
this.dirty=true;
this.trailingSymbol="...";
this._events=[];
},createGroup:function(_7){
if(!_7){
_7=this.chart.surface;
}
if(!this.group){
this.group=_7.createGroup();
}
return this;
},purgeGroup:function(){
this.destroyHtmlElements();
if(this.group){
this.group.clear();
this.group.removeShape();
this.group=null;
}
this.dirty=true;
if(this._events.length){
_2.forEach(this._events,function(_8){
_8.shape.disconnect(_8.handle);
});
this._events=[];
}
return this;
},cleanGroup:function(_9){
this.destroyHtmlElements();
if(!_9){
_9=this.chart.surface;
}
if(this.group){
this.group.clear();
}else{
this.group=_9.createGroup();
}
this.dirty=true;
return this;
},destroyHtmlElements:function(){
if(this.htmlElements.length){
_2.forEach(this.htmlElements,_3.destroy);
this.htmlElements=[];
}
},destroy:function(){
this.purgeGroup();
},getTextWidth:function(s,_a){
return _5._base._getTextBox(s,{font:_a}).w||0;
},getTextWithLimitLength:function(s,_b,_c,_d){
if(!s||s.length<=0){
return {text:"",truncated:_d||false};
}
if(!_c||_c<=0){
return {text:s,truncated:_d||false};
}
var _e=2,_f=0.618,_10=s.substring(0,1)+this.trailingSymbol,_11=this.getTextWidth(_10,_b);
if(_c<=_11){
return {text:_10,truncated:true};
}
var _12=this.getTextWidth(s,_b);
if(_12<=_c){
return {text:s,truncated:_d||false};
}else{
var _13=0,end=s.length;
while(_13<end){
if(end-_13<=_e){
while(this.getTextWidth(s.substring(0,_13)+this.trailingSymbol,_b)>_c){
_13-=1;
}
return {text:(s.substring(0,_13)+this.trailingSymbol),truncated:true};
}
var _14=_13+Math.round((end-_13)*_f),_15=this.getTextWidth(s.substring(0,_14),_b);
if(_15<_c){
_13=_14;
end=end;
}else{
_13=_13;
end=_14;
}
}
}
},getTextWithLimitCharCount:function(s,_16,_17,_18){
if(!s||s.length<=0){
return {text:"",truncated:_18||false};
}
if(!_17||_17<=0||s.length<=_17){
return {text:s,truncated:_18||false};
}
return {text:s.substring(0,_17)+this.trailingSymbol,truncated:true};
},_plotFill:function(_19,dim,_1a){
if(!_19||!_19.type||!_19.space){
return _19;
}
var _1b=_19.space;
switch(_19.type){
case "linear":
if(_1b==="plot"||_1b==="shapeX"||_1b==="shapeY"){
_19=_5.makeParameters(_5.defaultLinearGradient,_19);
_19.space=_1b;
if(_1b==="plot"||_1b==="shapeX"){
var _1c=dim.height-_1a.t-_1a.b;
_19.y1=_1a.t+_1c*_19.y1/100;
_19.y2=_1a.t+_1c*_19.y2/100;
}
if(_1b==="plot"||_1b==="shapeY"){
var _1c=dim.width-_1a.l-_1a.r;
_19.x1=_1a.l+_1c*_19.x1/100;
_19.x2=_1a.l+_1c*_19.x2/100;
}
}
break;
case "radial":
if(_1b==="plot"){
_19=_5.makeParameters(_5.defaultRadialGradient,_19);
_19.space=_1b;
var _1d=dim.width-_1a.l-_1a.r,_1e=dim.height-_1a.t-_1a.b;
_19.cx=_1a.l+_1d*_19.cx/100;
_19.cy=_1a.t+_1e*_19.cy/100;
_19.r=_19.r*Math.sqrt(_1d*_1d+_1e*_1e)/200;
}
break;
case "pattern":
if(_1b==="plot"||_1b==="shapeX"||_1b==="shapeY"){
_19=_5.makeParameters(_5.defaultPattern,_19);
_19.space=_1b;
if(_1b==="plot"||_1b==="shapeX"){
var _1c=dim.height-_1a.t-_1a.b;
_19.y=_1a.t+_1c*_19.y/100;
_19.height=_1c*_19.height/100;
}
if(_1b==="plot"||_1b==="shapeY"){
var _1c=dim.width-_1a.l-_1a.r;
_19.x=_1a.l+_1c*_19.x/100;
_19.width=_1c*_19.width/100;
}
}
break;
}
return _19;
},_shapeFill:function(_1f,_20){
if(!_1f||!_1f.space){
return _1f;
}
var _21=_1f.space;
switch(_1f.type){
case "linear":
if(_21==="shape"||_21==="shapeX"||_21==="shapeY"){
_1f=_5.makeParameters(_5.defaultLinearGradient,_1f);
_1f.space=_21;
if(_21==="shape"||_21==="shapeX"){
var _22=_20.width;
_1f.x1=_20.x+_22*_1f.x1/100;
_1f.x2=_20.x+_22*_1f.x2/100;
}
if(_21==="shape"||_21==="shapeY"){
var _22=_20.height;
_1f.y1=_20.y+_22*_1f.y1/100;
_1f.y2=_20.y+_22*_1f.y2/100;
}
}
break;
case "radial":
if(_21==="shape"){
_1f=_5.makeParameters(_5.defaultRadialGradient,_1f);
_1f.space=_21;
_1f.cx=_20.x+_20.width/2;
_1f.cy=_20.y+_20.height/2;
_1f.r=_1f.r*_20.width/200;
}
break;
case "pattern":
if(_21==="shape"||_21==="shapeX"||_21==="shapeY"){
_1f=_5.makeParameters(_5.defaultPattern,_1f);
_1f.space=_21;
if(_21==="shape"||_21==="shapeX"){
var _22=_20.width;
_1f.x=_20.x+_22*_1f.x/100;
_1f.width=_22*_1f.width/100;
}
if(_21==="shape"||_21==="shapeY"){
var _22=_20.height;
_1f.y=_20.y+_22*_1f.y/100;
_1f.height=_22*_1f.height/100;
}
}
break;
}
return _1f;
},_pseudoRadialFill:function(_23,_24,_25,_26,end){
if(!_23||_23.type!=="radial"||_23.space!=="shape"){
return _23;
}
var _27=_23.space;
_23=_5.makeParameters(_5.defaultRadialGradient,_23);
_23.space=_27;
if(arguments.length<4){
_23.cx=_24.x;
_23.cy=_24.y;
_23.r=_23.r*_25/100;
return _23;
}
var _28=arguments.length<5?_26:(end+_26)/2;
return {type:"linear",x1:_24.x,y1:_24.y,x2:_24.x+_23.r*_25*Math.cos(_28)/100,y2:_24.y+_23.r*_25*Math.sin(_28)/100,colors:_23.colors};
return _23;
}});
});
