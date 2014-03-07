//>>built
define("dojox/charting/plot2d/Pie",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","../Element","./_PlotEvents","./common","../axis2d/common","dojox/gfx","dojox/gfx/matrix","dojox/lang/functional","dojox/lang/utils"],function(_1,_2,_3,_4,_5,dc,da,g,m,df,du){
var _6=0.2;
return _3("dojox.charting.plot2d.Pie",[_4,_5],{defaultParams:{labels:true,ticks:false,fixed:true,precision:1,labelOffset:20,labelStyle:"default",htmlLabels:true,radGrad:"native",fanSize:5,startAngle:0},optionalParams:{radius:0,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:"",labelWiring:{}},constructor:function(_7,_8){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_8);
du.updateWithPattern(this.opt,_8,this.optionalParams);
this.run=null;
this.dyn=[];
},clear:function(){
this.dirty=true;
this.dyn=[];
this.run=null;
return this;
},setAxis:function(_9){
return this;
},addSeries:function(_a){
this.run=_a;
return this;
},getSeriesStats:function(){
return _1.delegate(dc.defaultStats);
},initializeScalers:function(){
return this;
},getRequiredColors:function(){
return this.run?this.run.data.length:0;
},render:function(_b,_c){
if(!this.dirty){
return this;
}
this.resetEvents();
this.dirty=false;
this._eventSeries={};
this.cleanGroup();
var s=this.group,t=this.chart.theme;
if(!this.run||!this.run.data.length){
return this;
}
var rx=(_b.width-_c.l-_c.r)/2,ry=(_b.height-_c.t-_c.b)/2,r=Math.min(rx,ry),_d="font" in this.opt?this.opt.font:t.axis.font,_e=_d?g.normalizedLength(g.splitFontString(_d).size):0,_f="fontColor" in this.opt?this.opt.fontColor:t.axis.fontColor,_10=m._degToRad(this.opt.startAngle),_11=_10,_12,_13,_14,_15,_16,_17,run=this.run.data,_18=this.events();
if(typeof run[0]=="number"){
_13=df.map(run,"x ? Math.max(x, 0) : 0");
if(df.every(_13,"<= 0")){
return this;
}
_14=df.map(_13,"/this",df.foldl(_13,"+",0));
if(this.opt.labels){
_15=_2.map(_14,function(x){
return x>0?this._getLabel(x*100)+"%":"";
},this);
}
}else{
_13=df.map(run,"x ? Math.max(x.y, 0) : 0");
if(df.every(_13,"<= 0")){
return this;
}
_14=df.map(_13,"/this",df.foldl(_13,"+",0));
if(this.opt.labels){
_15=_2.map(_14,function(x,i){
if(x<=0){
return "";
}
var v=run[i];
return "text" in v?v.text:this._getLabel(x*100)+"%";
},this);
}
}
var _19=df.map(run,function(v,i){
if(v===null||typeof v=="number"){
return t.next("slice",[this.opt,this.run],true);
}
return t.next("slice",[this.opt,this.run,v],true);
},this);
if(this.opt.labels){
_16=df.foldl1(df.map(_15,function(_1a,i){
var _1b=_19[i].series.font;
return g._base._getTextBox(_1a,{font:_1b}).w;
},this),"Math.max(a, b)")/2;
if(this.opt.labelOffset<0){
r=Math.min(rx-2*_16,ry-_e)+this.opt.labelOffset;
}
_17=r-this.opt.labelOffset;
}
if("radius" in this.opt){
r=this.opt.radius;
_17=r-this.opt.labelOffset;
}
var _1c={cx:_c.l+rx,cy:_c.t+ry,r:r};
this.dyn=[];
var _1d=new Array(_14.length);
_2.some(_14,function(_1e,i){
if(_1e<0){
return false;
}
if(_1e==0){
this.dyn.push({fill:null,stroke:null});
return false;
}
var v=run[i],_1f=_19[i],_20;
if(_1e>=1){
_20=this._plotFill(_1f.series.fill,_b,_c);
_20=this._shapeFill(_20,{x:_1c.cx-_1c.r,y:_1c.cy-_1c.r,width:2*_1c.r,height:2*_1c.r});
_20=this._pseudoRadialFill(_20,{x:_1c.cx,y:_1c.cy},_1c.r);
var _21=s.createCircle(_1c).setFill(_20).setStroke(_1f.series.stroke);
this.dyn.push({fill:_20,stroke:_1f.series.stroke});
if(_18){
var o={element:"slice",index:i,run:this.run,shape:_21,x:i,y:typeof v=="number"?v:v.y,cx:_1c.cx,cy:_1c.cy,cr:r};
this._connectEvents(o);
_1d[i]=o;
}
return true;
}
var end=_11+_1e*2*Math.PI;
if(i+1==_14.length){
end=_10+2*Math.PI;
}
var _22=end-_11,x1=_1c.cx+r*Math.cos(_11),y1=_1c.cy+r*Math.sin(_11),x2=_1c.cx+r*Math.cos(end),y2=_1c.cy+r*Math.sin(end);
var _23=m._degToRad(this.opt.fanSize);
if(_1f.series.fill&&_1f.series.fill.type==="radial"&&this.opt.radGrad==="fan"&&_22>_23){
var _24=s.createGroup(),_25=Math.ceil(_22/_23),_26=_22/_25;
_20=this._shapeFill(_1f.series.fill,{x:_1c.cx-_1c.r,y:_1c.cy-_1c.r,width:2*_1c.r,height:2*_1c.r});
for(var j=0;j<_25;++j){
var _27=j==0?x1:_1c.cx+r*Math.cos(_11+(j-_6)*_26),_28=j==0?y1:_1c.cy+r*Math.sin(_11+(j-_6)*_26),_29=j==_25-1?x2:_1c.cx+r*Math.cos(_11+(j+1+_6)*_26),_2a=j==_25-1?y2:_1c.cy+r*Math.sin(_11+(j+1+_6)*_26),fan=_24.createPath().moveTo(_1c.cx,_1c.cy).lineTo(_27,_28).arcTo(r,r,0,_26>Math.PI,true,_29,_2a).lineTo(_1c.cx,_1c.cy).closePath().setFill(this._pseudoRadialFill(_20,{x:_1c.cx,y:_1c.cy},r,_11+(j+0.5)*_26,_11+(j+0.5)*_26));
}
_24.createPath().moveTo(_1c.cx,_1c.cy).lineTo(x1,y1).arcTo(r,r,0,_22>Math.PI,true,x2,y2).lineTo(_1c.cx,_1c.cy).closePath().setStroke(_1f.series.stroke);
_21=_24;
}else{
_21=s.createPath().moveTo(_1c.cx,_1c.cy).lineTo(x1,y1).arcTo(r,r,0,_22>Math.PI,true,x2,y2).lineTo(_1c.cx,_1c.cy).closePath().setStroke(_1f.series.stroke);
var _20=_1f.series.fill;
if(_20&&_20.type==="radial"){
_20=this._shapeFill(_20,{x:_1c.cx-_1c.r,y:_1c.cy-_1c.r,width:2*_1c.r,height:2*_1c.r});
if(this.opt.radGrad==="linear"){
_20=this._pseudoRadialFill(_20,{x:_1c.cx,y:_1c.cy},r,_11,end);
}
}else{
if(_20&&_20.type==="linear"){
_20=this._plotFill(_20,_b,_c);
_20=this._shapeFill(_20,_21.getBoundingBox());
}
}
_21.setFill(_20);
}
this.dyn.push({fill:_20,stroke:_1f.series.stroke});
if(_18){
var o={element:"slice",index:i,run:this.run,shape:_21,x:i,y:typeof v=="number"?v:v.y,cx:_1c.cx,cy:_1c.cy,cr:r};
this._connectEvents(o);
_1d[i]=o;
}
_11=end;
return false;
},this);
if(this.opt.labels){
if(this.opt.labelStyle=="default"){
_11=_10;
_2.some(_14,function(_2b,i){
if(_2b<=0){
return false;
}
var _2c=_19[i];
if(_2b>=1){
var v=run[i],_2d=da.createText[this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx"](this.chart,s,_1c.cx,_1c.cy+_e/2,"middle",_15[i],_2c.series.font,_2c.series.fontColor);
if(this.opt.htmlLabels){
this.htmlElements.push(_2d);
}
return true;
}
var end=_11+_2b*2*Math.PI,v=run[i];
if(i+1==_14.length){
end=_10+2*Math.PI;
}
var _2e=(_11+end)/2,x=_1c.cx+_17*Math.cos(_2e),y=_1c.cy+_17*Math.sin(_2e)+_e/2;
var _2d=da.createText[this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx"](this.chart,s,x,y,"middle",_15[i],_2c.series.font,_2c.series.fontColor);
if(this.opt.htmlLabels){
this.htmlElements.push(_2d);
}
_11=end;
return false;
},this);
}else{
if(this.opt.labelStyle=="columns"){
_11=_10;
var _2f=[];
_2.forEach(_14,function(_30,i){
var end=_11+_30*2*Math.PI;
if(i+1==_14.length){
end=_10+2*Math.PI;
}
var _31=(_11+end)/2;
_2f.push({angle:_31,left:Math.cos(_31)<0,theme:_19[i],index:i,omit:end-_11<0.001});
_11=end;
});
var _32=g._base._getTextBox("a",{font:_d}).h;
this._getProperLabelRadius(_2f,_32,_1c.r*1.1);
_2.forEach(_2f,function(_33,i){
if(!_33.omit){
var _34=_1c.cx-_1c.r*2,_35=_1c.cx+_1c.r*2,_36=g._base._getTextBox(_15[i],{font:_d}).w,x=_1c.cx+_33.labelR*Math.cos(_33.angle),y=_1c.cy+_33.labelR*Math.sin(_33.angle),_37=(_33.left)?(_34+_36):(_35-_36),_38=(_33.left)?_34:_37;
var _39=s.createPath().moveTo(_1c.cx+_1c.r*Math.cos(_33.angle),_1c.cy+_1c.r*Math.sin(_33.angle));
if(Math.abs(_33.labelR*Math.cos(_33.angle))<_1c.r*2-_36){
_39.lineTo(x,y);
}
_39.lineTo(_37,y).setStroke(_33.theme.series.labelWiring);
var _3a=da.createText[this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx"](this.chart,s,_38,y,"left",_15[i],_33.theme.series.font,_33.theme.series.fontColor);
if(this.opt.htmlLabels){
this.htmlElements.push(_3a);
}
}
},this);
}
}
}
var esi=0;
this._eventSeries[this.run.name]=df.map(run,function(v){
return v<=0?null:_1d[esi++];
});
return this;
},_getProperLabelRadius:function(_3b,_3c,_3d){
var _3e={},_3f={},_40=1,_41=1;
if(_3b.length==1){
_3b[0].labelR=_3d;
return;
}
for(var i=0;i<_3b.length;i++){
var _42=Math.abs(Math.sin(_3b[i].angle));
if(_3b[i].left){
if(_40>_42){
_40=_42;
_3e=_3b[i];
}
}else{
if(_41>_42){
_41=_42;
_3f=_3b[i];
}
}
}
_3e.labelR=_3f.labelR=_3d;
this._calculateLabelR(_3e,_3b,_3c);
this._calculateLabelR(_3f,_3b,_3c);
},_calculateLabelR:function(_43,_44,_45){
var i=_43.index,_46=_44.length,_47=_43.labelR;
while(!(_44[i%_46].left^_44[(i+1)%_46].left)){
if(!_44[(i+1)%_46].omit){
var _48=(Math.sin(_44[i%_46].angle)*_47+((_44[i%_46].left)?(-_45):_45))/Math.sin(_44[(i+1)%_46].angle);
_47=(_48<_43.labelR)?_43.labelR:_48;
_44[(i+1)%_46].labelR=_47;
}
i++;
}
i=_43.index;
var j=(i==0)?_46-1:i-1;
while(!(_44[i].left^_44[j].left)){
if(!_44[j].omit){
var _48=(Math.sin(_44[i].angle)*_47+((_44[i].left)?_45:(-_45)))/Math.sin(_44[j].angle);
_47=(_48<_43.labelR)?_43.labelR:_48;
_44[j].labelR=_47;
}
i--;
j--;
i=(i<0)?i+_44.length:i;
j=(j<0)?j+_44.length:j;
}
},_getLabel:function(_49){
return dc.getLabel(_49,this.opt.fixed,this.opt.precision);
}});
});
