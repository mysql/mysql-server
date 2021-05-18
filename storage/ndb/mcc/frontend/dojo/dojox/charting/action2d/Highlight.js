//>>built
define("dojox/charting/action2d/Highlight",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","dojo/_base/connect","dojox/color/_base","./PlotAction","dojo/fx/easing","dojox/gfx/fx"],function(_1,_2,_3,_4,c,_5,_6,_7){
var _8=100,_9=75,_a=50,cc=function(_b){
return function(){
return _b;
};
},hl=function(_c){
var a=new c.Color(_c),x=a.toHsl();
if(x.s==0){
x.l=x.l<50?100:0;
}else{
x.s=_8;
if(x.l<_a){
x.l=_9;
}else{
if(x.l>_9){
x.l=_a;
}else{
x.l=x.l-_a>_9-x.l?_a:_9;
}
}
}
var _d=c.fromHsl(x);
_d.a=a.a;
return _d;
},_e=function(_f){
var r=hl(_f);
r.a=0.7;
return r;
};
return _2("dojox.charting.action2d.Highlight",_5,{defaultParams:{duration:400,easing:_6.backOut},optionalParams:{highlight:"red"},constructor:function(_10,_11,_12){
var a=_12&&_12.highlight;
this.colorFunc=a?(_1.isFunction(a)?a:cc(a)):hl;
this.connect();
},process:function(o){
if(!o.shape||!(o.type in this.overOutEvents)){
return;
}
if(o.element=="spider_circle"||o.element=="spider_plot"){
return;
}else{
if(o.element=="spider_poly"&&this.colorFunc==hl){
this.colorFunc=_e;
}
}
var _13=o.run.name,_14=o.index,_15;
if(_13 in this.anim){
_15=this.anim[_13][_14];
}else{
this.anim[_13]={};
}
if(_15){
_15.action.stop(true);
}else{
var _16=o.shape.getFill();
if(!_16||!(_16 instanceof _3)){
return;
}
this.anim[_13][_14]=_15={start:_16,end:this.colorFunc(_16)};
}
var _17=_15.start,end=_15.end;
if(o.type=="onmouseout"){
var t=_17;
_17=end;
end=t;
}
_15.action=_7.animateFill({shape:o.shape,duration:this.duration,easing:this.easing,color:{start:_17,end:end}});
if(o.type=="onmouseout"){
_4.connect(_15.action,"onEnd",this,function(){
if(this.anim[_13]){
delete this.anim[_13][_14];
}
});
}
_15.action.play();
}});
});
