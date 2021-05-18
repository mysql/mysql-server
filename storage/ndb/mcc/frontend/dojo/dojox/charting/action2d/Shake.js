//>>built
define("dojox/charting/action2d/Shake",["dojo/_base/connect","dojo/_base/declare","./PlotAction","dojo/fx","dojo/fx/easing","dojox/gfx/matrix","dojox/gfx/fx"],function(_1,_2,_3,df,_4,m,gf){
var _5=3;
return _2("dojox.charting.action2d.Shake",_3,{defaultParams:{duration:400,easing:_4.backOut,shiftX:_5,shiftY:_5},optionalParams:{},constructor:function(_6,_7,_8){
if(!_8){
_8={};
}
this.shiftX=typeof _8.shiftX=="number"?_8.shiftX:_5;
this.shiftY=typeof _8.shiftY=="number"?_8.shiftY:_5;
this.connect();
},process:function(o){
if(!o.shape||!(o.type in this.overOutEvents)){
return;
}
var _9=o.run.name,_a=o.index,_b=[],_c;
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
var _d={shape:o.shape,duration:this.duration,easing:this.easing,transform:[{name:"translate",start:[this.shiftX,this.shiftY],end:[0,0]},m.identity]};
if(o.shape){
_b.push(gf.animateTransform(_d));
}
if(o.oultine){
_d.shape=o.outline;
_b.push(gf.animateTransform(_d));
}
if(o.shadow){
_d.shape=o.shadow;
_b.push(gf.animateTransform(_d));
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
