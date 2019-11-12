//>>built
define("dojox/charting/action2d/MoveSlice",["dojo/_base/connect","dojo/_base/declare","dojo/_base/array","./PlotAction","dojo/fx/easing","dojox/gfx/matrix","dojox/gfx/fx","dojox/lang/functional","dojox/lang/functional/scan","dojox/lang/functional/fold"],function(_1,_2,_3,_4,_5,m,gf,df,_6,_7){
var _8=1.05,_9=7;
return _2("dojox.charting.action2d.MoveSlice",_4,{defaultParams:{duration:400,easing:_5.backOut,scale:_8,shift:_9},optionalParams:{},constructor:function(_a,_b,_c){
if(!_c){
_c={};
}
this.scale=typeof _c.scale=="number"?_c.scale:_8;
this.shift=typeof _c.shift=="number"?_c.shift:_9;
this.connect();
},process:function(o){
if(!o.shape||o.element!="slice"||!(o.type in this.overOutEvents)){
return;
}
if(!this.angles){
var _d=m._degToRad(o.plot.opt.startAngle);
if(typeof o.run.data[0]=="number"){
this.angles=df.map(df.scanl(o.run.data,"+",0),"* 2 * Math.PI / this",df.foldl(o.run.data,"+",0));
}else{
this.angles=df.map(df.scanl(o.run.data,"a + b.y",0),"* 2 * Math.PI / this",df.foldl(o.run.data,"a + b.y",0));
}
this.angles=_3.map(this.angles,function(_e){
return _e+_d;
});
}
var _f=o.index,_10,_11,_12,_13,_14,_15=(this.angles[_f]+this.angles[_f+1])/2,_16=m.rotateAt(-_15,o.cx,o.cy),_17=m.rotateAt(_15,o.cx,o.cy);
_10=this.anim[_f];
if(_10){
_10.action.stop(true);
}else{
this.anim[_f]=_10={};
}
if(o.type=="onmouseover"){
_13=0;
_14=this.shift;
_11=1;
_12=this.scale;
}else{
_13=this.shift;
_14=0;
_11=this.scale;
_12=1;
}
_10.action=gf.animateTransform({shape:o.shape,duration:this.duration,easing:this.easing,transform:[_17,{name:"translate",start:[_13,0],end:[_14,0]},{name:"scaleAt",start:[_11,o.cx,o.cy],end:[_12,o.cx,o.cy]},_16]});
if(o.type=="onmouseout"){
_1.connect(_10.action,"onEnd",this,function(){
delete this.anim[_f];
});
}
_10.action.play();
},reset:function(){
delete this.angles;
}});
});
