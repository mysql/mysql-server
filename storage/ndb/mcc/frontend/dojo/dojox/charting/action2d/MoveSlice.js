//>>built
define("dojox/charting/action2d/MoveSlice",["dojo/_base/connect","dojo/_base/declare","./PlotAction","dojo/fx/easing","dojox/gfx/matrix","dojox/gfx/fx","dojox/lang/functional","dojox/lang/functional/scan","dojox/lang/functional/fold"],function(_1,_2,_3,_4,m,gf,df,_5,_6){
var _7=1.05,_8=7;
return _2("dojox.charting.action2d.MoveSlice",_3,{defaultParams:{duration:400,easing:_4.backOut,scale:_7,shift:_8},optionalParams:{},constructor:function(_9,_a,_b){
if(!_b){
_b={};
}
this.scale=typeof _b.scale=="number"?_b.scale:_7;
this.shift=typeof _b.shift=="number"?_b.shift:_8;
this.connect();
},process:function(o){
if(!o.shape||o.element!="slice"||!(o.type in this.overOutEvents)){
return;
}
if(!this.angles){
var _c=m._degToRad(o.plot.opt.startAngle);
if(typeof o.run.data[0]=="number"){
this.angles=df.map(df.scanl(o.run.data,"+",_c),"* 2 * Math.PI / this",df.foldl(o.run.data,"+",0));
}else{
this.angles=df.map(df.scanl(o.run.data,"a + b.y",_c),"* 2 * Math.PI / this",df.foldl(o.run.data,"a + b.y",0));
}
}
var _d=o.index,_e,_f,_10,_11,_12,_13=(this.angles[_d]+this.angles[_d+1])/2,_14=m.rotateAt(-_13,o.cx,o.cy),_15=m.rotateAt(_13,o.cx,o.cy);
_e=this.anim[_d];
if(_e){
_e.action.stop(true);
}else{
this.anim[_d]=_e={};
}
if(o.type=="onmouseover"){
_11=0;
_12=this.shift;
_f=1;
_10=this.scale;
}else{
_11=this.shift;
_12=0;
_f=this.scale;
_10=1;
}
_e.action=gf.animateTransform({shape:o.shape,duration:this.duration,easing:this.easing,transform:[_15,{name:"translate",start:[_11,0],end:[_12,0]},{name:"scaleAt",start:[_f,o.cx,o.cy],end:[_10,o.cx,o.cy]},_14]});
if(o.type=="onmouseout"){
_1.connect(_e.action,"onEnd",this,function(){
delete this.anim[_d];
});
}
_e.action.play();
},reset:function(){
delete this.angles;
}});
});
