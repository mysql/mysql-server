//>>built
define("dojox/charting/Element",["dojo/_base/array","dojo/dom-construct","dojo/_base/declare","dojox/gfx","dojox/gfx/shape"],function(_1,_2,_3,_4,_5){
return _3("dojox.charting.Element",null,{chart:null,group:null,htmlElements:null,dirty:true,renderingOptions:null,constructor:function(_6,_7){
this.chart=_6;
this.group=null;
this.htmlElements=[];
this.dirty=true;
this.trailingSymbol="...";
this._events=[];
if(_7&&_7.renderingOptions){
this.renderingOptions=_7.renderingOptions;
}
},purgeGroup:function(){
this.destroyHtmlElements();
if(this.group){
this.getGroup().removeShape();
var _8=this.getGroup().children;
if(_5.dispose){
for(var i=0;i<_8.length;++i){
_5.dispose(_8[i],true);
}
}
if(this.getGroup().rawNode){
_2.empty(this.getGroup().rawNode);
}
this.getGroup().clear();
if(_5.dispose){
_5.dispose(this.getGroup(),true);
}
if(this.getGroup()!=this.group){
if(this.group.rawNode){
_2.empty(this.group.rawNode);
}
this.group.clear();
if(_5.dispose){
_5.dispose(this.group,true);
}
}
this.group=null;
}
this.dirty=true;
if(this._events.length){
_1.forEach(this._events,function(_9){
_9.shape.disconnect(_9.handle);
});
this._events=[];
}
return this;
},cleanGroup:function(_a){
this.destroyHtmlElements();
if(!_a){
_a=this.chart.surface;
}
if(this.group){
var _b;
var _c=this.getGroup().children;
if(_5.dispose){
for(var i=0;i<_c.length;++i){
_5.dispose(_c[i],true);
}
}
if(this.getGroup().rawNode){
_b=this.getGroup().bgNode;
_2.empty(this.getGroup().rawNode);
}
this.getGroup().clear();
if(_b){
this.getGroup().rawNode.appendChild(_b);
}
}else{
this.group=_a.createGroup();
if(this.renderingOptions&&this.group.rawNode&&this.group.rawNode.namespaceURI=="http://www.w3.org/2000/svg"){
for(var _d in this.renderingOptions){
this.group.rawNode.setAttribute(_d,this.renderingOptions[_d]);
}
}
}
this.dirty=true;
return this;
},getGroup:function(){
return this.group;
},destroyHtmlElements:function(){
if(this.htmlElements.length){
_1.forEach(this.htmlElements,_2.destroy);
this.htmlElements=[];
}
},destroy:function(){
this.purgeGroup();
},overrideShape:function(_e,_f){
},getTextWidth:function(s,_10){
return _4._base._getTextBox(s,{font:_10}).w||0;
},getTextWithLimitLength:function(s,_11,_12,_13){
if(!s||s.length<=0){
return {text:"",truncated:_13||false};
}
if(!_12||_12<=0){
return {text:s,truncated:_13||false};
}
var _14=2,_15=0.618,_16=s.substring(0,1)+this.trailingSymbol,_17=this.getTextWidth(_16,_11);
if(_12<=_17){
return {text:_16,truncated:true};
}
var _18=this.getTextWidth(s,_11);
if(_18<=_12){
return {text:s,truncated:_13||false};
}else{
var _19=0,end=s.length;
while(_19<end){
if(end-_19<=_14){
while(this.getTextWidth(s.substring(0,_19)+this.trailingSymbol,_11)>_12){
_19-=1;
}
return {text:(s.substring(0,_19)+this.trailingSymbol),truncated:true};
}
var _1a=_19+Math.round((end-_19)*_15),_1b=this.getTextWidth(s.substring(0,_1a),_11);
if(_1b<_12){
_19=_1a;
end=end;
}else{
_19=_19;
end=_1a;
}
}
}
},getTextWithLimitCharCount:function(s,_1c,_1d,_1e){
if(!s||s.length<=0){
return {text:"",truncated:_1e||false};
}
if(!_1d||_1d<=0||s.length<=_1d){
return {text:s,truncated:_1e||false};
}
return {text:s.substring(0,_1d)+this.trailingSymbol,truncated:true};
},_plotFill:function(_1f,dim,_20){
if(!_1f||!_1f.type||!_1f.space){
return _1f;
}
var _21=_1f.space,_22;
switch(_1f.type){
case "linear":
if(_21==="plot"||_21==="shapeX"||_21==="shapeY"){
_1f=_4.makeParameters(_4.defaultLinearGradient,_1f);
_1f.space=_21;
if(_21==="plot"||_21==="shapeX"){
_22=dim.height-_20.t-_20.b;
_1f.y1=_20.t+_22*_1f.y1/100;
_1f.y2=_20.t+_22*_1f.y2/100;
}
if(_21==="plot"||_21==="shapeY"){
_22=dim.width-_20.l-_20.r;
_1f.x1=_20.l+_22*_1f.x1/100;
_1f.x2=_20.l+_22*_1f.x2/100;
}
}
break;
case "radial":
if(_21==="plot"){
_1f=_4.makeParameters(_4.defaultRadialGradient,_1f);
_1f.space=_21;
var _23=dim.width-_20.l-_20.r,_24=dim.height-_20.t-_20.b;
_1f.cx=_20.l+_23*_1f.cx/100;
_1f.cy=_20.t+_24*_1f.cy/100;
_1f.r=_1f.r*Math.sqrt(_23*_23+_24*_24)/200;
}
break;
case "pattern":
if(_21==="plot"||_21==="shapeX"||_21==="shapeY"){
_1f=_4.makeParameters(_4.defaultPattern,_1f);
_1f.space=_21;
if(_21==="plot"||_21==="shapeX"){
_22=dim.height-_20.t-_20.b;
_1f.y=_20.t+_22*_1f.y/100;
_1f.height=_22*_1f.height/100;
}
if(_21==="plot"||_21==="shapeY"){
_22=dim.width-_20.l-_20.r;
_1f.x=_20.l+_22*_1f.x/100;
_1f.width=_22*_1f.width/100;
}
}
break;
}
return _1f;
},_shapeFill:function(_25,_26){
if(!_25||!_25.space){
return _25;
}
var _27=_25.space,_28;
switch(_25.type){
case "linear":
if(_27==="shape"||_27==="shapeX"||_27==="shapeY"){
_25=_4.makeParameters(_4.defaultLinearGradient,_25);
_25.space=_27;
if(_27==="shape"||_27==="shapeX"){
_28=_26.width;
_25.x1=_26.x+_28*_25.x1/100;
_25.x2=_26.x+_28*_25.x2/100;
}
if(_27==="shape"||_27==="shapeY"){
_28=_26.height;
_25.y1=_26.y+_28*_25.y1/100;
_25.y2=_26.y+_28*_25.y2/100;
}
}
break;
case "radial":
if(_27==="shape"){
_25=_4.makeParameters(_4.defaultRadialGradient,_25);
_25.space=_27;
_25.cx=_26.x+_26.width/2;
_25.cy=_26.y+_26.height/2;
_25.r=_25.r*_26.width/200;
}
break;
case "pattern":
if(_27==="shape"||_27==="shapeX"||_27==="shapeY"){
_25=_4.makeParameters(_4.defaultPattern,_25);
_25.space=_27;
if(_27==="shape"||_27==="shapeX"){
_28=_26.width;
_25.x=_26.x+_28*_25.x/100;
_25.width=_28*_25.width/100;
}
if(_27==="shape"||_27==="shapeY"){
_28=_26.height;
_25.y=_26.y+_28*_25.y/100;
_25.height=_28*_25.height/100;
}
}
break;
}
return _25;
},_pseudoRadialFill:function(_29,_2a,_2b,_2c,end){
if(!_29||_29.type!=="radial"||_29.space!=="shape"){
return _29;
}
var _2d=_29.space;
_29=_4.makeParameters(_4.defaultRadialGradient,_29);
_29.space=_2d;
if(arguments.length<4){
_29.cx=_2a.x;
_29.cy=_2a.y;
_29.r=_29.r*_2b/100;
return _29;
}
var _2e=arguments.length<5?_2c:(end+_2c)/2;
return {type:"linear",x1:_2a.x,y1:_2a.y,x2:_2a.x+_29.r*_2b*Math.cos(_2e)/100,y2:_2a.y+_29.r*_2b*Math.sin(_2e)/100,colors:_29.colors};
}});
});
