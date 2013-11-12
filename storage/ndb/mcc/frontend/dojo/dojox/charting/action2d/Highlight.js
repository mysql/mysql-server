//>>built
define("dojox/charting/action2d/Highlight",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","dojo/_base/connect","dojox/color/_base","./PlotAction","dojo/fx/easing","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,c,_6,_7,_8){
var _9=100,_a=75,_b=50,cc=function(_c){
return function(){
return _c;
};
},hl=function(_d){
var a=new c.Color(_d),x=a.toHsl();
if(x.s==0){
x.l=x.l<50?100:0;
}else{
x.s=_9;
if(x.l<_b){
x.l=_a;
}else{
if(x.l>_a){
x.l=_b;
}else{
x.l=x.l-_b>_a-x.l?_b:_a;
}
}
}
return c.fromHsl(x);
};
return _3("dojox.charting.action2d.Highlight",_6,{defaultParams:{duration:400,easing:_7.backOut},optionalParams:{highlight:"red"},constructor:function(_e,_f,_10){
var a=_10&&_10.highlight;
this.colorFun=a?(_2.isFunction(a)?a:cc(a)):hl;
this.connect();
},process:function(o){
if(!o.shape||!(o.type in this.overOutEvents)){
return;
}
var _11=o.run.name,_12=o.index,_13,_14,_15;
if(_11 in this.anim){
_13=this.anim[_11][_12];
}else{
this.anim[_11]={};
}
if(_13){
_13.action.stop(true);
}else{
var _16=o.shape.getFill();
if(!_16||!(_16 instanceof _4)){
return;
}
this.anim[_11][_12]=_13={start:_16,end:this.colorFun(_16)};
}
var _17=_13.start,end=_13.end;
if(o.type=="onmouseout"){
var t=_17;
_17=end;
end=t;
}
_13.action=_8.animateFill({shape:o.shape,duration:this.duration,easing:this.easing,color:{start:_17,end:end}});
if(o.type=="onmouseout"){
_5.connect(_13.action,"onEnd",this,function(){
if(this.anim[_11]){
delete this.anim[_11][_12];
}
});
}
_13.action.play();
}});
});
