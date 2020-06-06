//>>built
define("dojox/charting/action2d/Magnify",["dojo/_base/connect","dojo/_base/declare","./PlotAction","dojox/gfx/matrix","dojox/gfx/fx","dojo/fx","dojo/fx/easing"],function(_1,_2,_3,m,gf,df,_4){
var _5=2;
return _2("dojox.charting.action2d.Magnify",_3,{defaultParams:{duration:400,easing:_4.backOut,scale:_5},optionalParams:{},constructor:function(_6,_7,_8){
this.scale=_8&&typeof _8.scale=="number"?_8.scale:_5;
this.connect();
},process:function(o){
if(!o.shape||!(o.type in this.overOutEvents)||!("cx" in o)||!("cy" in o)){
return;
}
if(o.element=="spider_plot"||o.element=="spider_poly"){
return;
}
var _9=o.run.name,_a=o.index,_b=[],_c,_d,_e;
if(_9 in this.anim){
_c=this.anim[_9][_a];
}else{
this.anim[_9]={};
}
if(_c){
_c.action.stop(true);
}else{
this.anim[_9][_a]=_c={};
}
if(o.type=="onmouseover"){
_d=m.identity;
_e=this.scale;
}else{
_d=m.scaleAt(this.scale,o.cx,o.cy);
_e=1/this.scale;
}
var _f={shape:o.shape,duration:this.duration,easing:this.easing,transform:[{name:"scaleAt",start:[1,o.cx,o.cy],end:[_e,o.cx,o.cy]},_d]};
if(o.shape){
_b.push(gf.animateTransform(_f));
}
if(o.outline){
_f.shape=o.outline;
_b.push(gf.animateTransform(_f));
}
if(o.shadow){
_f.shape=o.shadow;
_b.push(gf.animateTransform(_f));
}
if(!_b.length){
delete this.anim[_9][_a];
return;
}
_c.action=df.combine(_b);
if(o.type=="onmouseout"){
_1.connect(_c.action,"onEnd",this,function(){
if(this.anim[_9]){
delete this.anim[_9][_a];
}
});
}
_c.action.play();
}});
});
