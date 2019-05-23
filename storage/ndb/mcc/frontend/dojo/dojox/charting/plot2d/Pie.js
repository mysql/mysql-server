//>>built
define("dojox/charting/plot2d/Pie",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./Base","./_PlotEvents","./common","../axis2d/common","dojox/gfx","dojox/gfx/matrix","dojox/lang/functional","dojox/lang/utils"],function(_1,_2,_3,_4,_5,dc,da,g,m,df,du){
var _6=0.2;
return _3("dojox.charting.plot2d.Pie",[_4,_5],{defaultParams:{labels:true,ticks:false,fixed:true,precision:1,labelOffset:20,labelStyle:"default",htmlLabels:true,radGrad:"native",fanSize:5,startAngle:0},optionalParams:{radius:0,omitLabels:false,stroke:{},outline:{},shadow:{},fill:{},styleFunc:null,font:"",fontColor:"",labelWiring:{}},constructor:function(_7,_8){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_8);
du.updateWithPattern(this.opt,_8,this.optionalParams);
this.axes=[];
this.run=null;
this.dyn=[];
},clear:function(){
this.inherited(arguments);
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
var rx=(_b.width-_c.l-_c.r)/2,ry=(_b.height-_c.t-_c.b)/2,r=Math.min(rx,ry),_d="font" in this.opt?this.opt.font:t.series.font,_e,_f=m._degToRad(this.opt.startAngle),_10=_f,_11,_12,_13,_14,_15,_16,run=this.run.data,_17=this.events();
this.dyn=[];
if("radius" in this.opt){
r=this.opt.radius;
_16=r-this.opt.labelOffset;
}
var _18={cx:_c.l+rx,cy:_c.t+ry,r:r};
if(this.opt.shadow||t.shadow){
var _19=this.opt.shadow||t.shadow;
var _1a=_1.clone(_18);
_1a.cx+=_19.dx;
_1a.cy+=_19.dy;
s.createCircle(_1a).setFill(_19.color).setStroke(_19);
}
if(typeof run[0]=="number"){
_12=df.map(run,"x ? Math.max(x, 0) : 0");
if(df.every(_12,"<= 0")){
s.createCircle(_18).setStroke(t.series.stroke);
this.dyn=_2.map(_12,function(){
return {};
});
return this;
}else{
_13=df.map(_12,"/this",df.foldl(_12,"+",0));
if(this.opt.labels){
_14=_2.map(_13,function(x){
return x>0?this._getLabel(x*100)+"%":"";
},this);
}
}
}else{
_12=df.map(run,"x ? Math.max(x.y, 0) : 0");
if(df.every(_12,"<= 0")){
s.createCircle(_18).setStroke(t.series.stroke);
this.dyn=_2.map(_12,function(){
return {};
});
return this;
}else{
_13=df.map(_12,"/this",df.foldl(_12,"+",0));
if(this.opt.labels){
_14=_2.map(_13,function(x,i){
if(x<0){
return "";
}
var v=run[i];
return "text" in v?v.text:this._getLabel(x*100)+"%";
},this);
}
}
}
var _1b=df.map(run,function(v,i){
var _1c=[this.opt,this.run];
if(v!==null&&typeof v!="number"){
_1c.push(v);
}
if(this.opt.styleFunc){
_1c.push(this.opt.styleFunc(v));
}
return t.next("slice",_1c,true);
},this);
if(this.opt.labels){
_e=_d?g.normalizedLength(g.splitFontString(_d).size):0;
_15=df.foldl1(df.map(_14,function(_1d,i){
var _1e=_1b[i].series.font;
return g._base._getTextBox(_1d,{font:_1e}).w;
},this),"Math.max(a, b)")/2;
if(this.opt.labelOffset<0){
r=Math.min(rx-2*_15,ry-_e)+this.opt.labelOffset;
}
_16=r-this.opt.labelOffset;
}
var _1f=new Array(_13.length);
_2.some(_13,function(_20,i){
if(_20<0){
return false;
}
if(_20==0){
this.dyn.push({fill:null,stroke:null});
return false;
}
var v=run[i],_21=_1b[i],_22,o;
if(_20>=1){
_22=this._plotFill(_21.series.fill,_b,_c);
_22=this._shapeFill(_22,{x:_18.cx-_18.r,y:_18.cy-_18.r,width:2*_18.r,height:2*_18.r});
_22=this._pseudoRadialFill(_22,{x:_18.cx,y:_18.cy},_18.r);
var _23=s.createCircle(_18).setFill(_22).setStroke(_21.series.stroke);
this.dyn.push({fill:_22,stroke:_21.series.stroke});
if(_17){
o={element:"slice",index:i,run:this.run,shape:_23,x:i,y:typeof v=="number"?v:v.y,cx:_18.cx,cy:_18.cy,cr:r};
this._connectEvents(o);
_1f[i]=o;
}
return true;
}
var end=_10+_20*2*Math.PI;
if(i+1==_13.length){
end=_f+2*Math.PI;
}
var _24=end-_10,x1=_18.cx+r*Math.cos(_10),y1=_18.cy+r*Math.sin(_10),x2=_18.cx+r*Math.cos(end),y2=_18.cy+r*Math.sin(end);
var _25=m._degToRad(this.opt.fanSize);
if(_21.series.fill&&_21.series.fill.type==="radial"&&this.opt.radGrad==="fan"&&_24>_25){
var _26=s.createGroup(),_27=Math.ceil(_24/_25),_28=_24/_27;
_22=this._shapeFill(_21.series.fill,{x:_18.cx-_18.r,y:_18.cy-_18.r,width:2*_18.r,height:2*_18.r});
for(var j=0;j<_27;++j){
var _29=j==0?x1:_18.cx+r*Math.cos(_10+(j-_6)*_28),_2a=j==0?y1:_18.cy+r*Math.sin(_10+(j-_6)*_28),_2b=j==_27-1?x2:_18.cx+r*Math.cos(_10+(j+1+_6)*_28),_2c=j==_27-1?y2:_18.cy+r*Math.sin(_10+(j+1+_6)*_28),fan=_26.createPath().moveTo(_18.cx,_18.cy).lineTo(_29,_2a).arcTo(r,r,0,_28>Math.PI,true,_2b,_2c).lineTo(_18.cx,_18.cy).closePath().setFill(this._pseudoRadialFill(_22,{x:_18.cx,y:_18.cy},r,_10+(j+0.5)*_28,_10+(j+0.5)*_28));
}
_26.createPath().moveTo(_18.cx,_18.cy).lineTo(x1,y1).arcTo(r,r,0,_24>Math.PI,true,x2,y2).lineTo(_18.cx,_18.cy).closePath().setStroke(_21.series.stroke);
_23=_26;
}else{
_23=s.createPath().moveTo(_18.cx,_18.cy).lineTo(x1,y1).arcTo(r,r,0,_24>Math.PI,true,x2,y2).lineTo(_18.cx,_18.cy).closePath().setStroke(_21.series.stroke);
_22=_21.series.fill;
if(_22&&_22.type==="radial"){
_22=this._shapeFill(_22,{x:_18.cx-_18.r,y:_18.cy-_18.r,width:2*_18.r,height:2*_18.r});
if(this.opt.radGrad==="linear"){
_22=this._pseudoRadialFill(_22,{x:_18.cx,y:_18.cy},r,_10,end);
}
}else{
if(_22&&_22.type==="linear"){
_22=this._plotFill(_22,_b,_c);
_22=this._shapeFill(_22,_23.getBoundingBox());
}
}
_23.setFill(_22);
}
this.dyn.push({fill:_22,stroke:_21.series.stroke});
if(_17){
o={element:"slice",index:i,run:this.run,shape:_23,x:i,y:typeof v=="number"?v:v.y,cx:_18.cx,cy:_18.cy,cr:r};
this._connectEvents(o);
_1f[i]=o;
}
_10=end;
return false;
},this);
if(this.opt.labels){
if(this.opt.labelStyle=="default"){
_10=_f;
_2.some(_13,function(_2d,i){
if(_2d<=0){
return false;
}
var _2e=_1b[i],_2f;
if(_2d>=1){
_2f=da.createText[this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx"](this.chart,s,_18.cx,_18.cy+_e/2,"middle",_14[i],_2e.series.font,_2e.series.fontColor);
if(this.opt.htmlLabels){
this.htmlElements.push(_2f);
}
return true;
}
var end=_10+_2d*2*Math.PI;
if(i+1==_13.length){
end=_f+2*Math.PI;
}
if(this.opt.omitLabels&&end-_10<0.001){
return false;
}
var _30=(_10+end)/2,x=_18.cx+_16*Math.cos(_30),y=_18.cy+_16*Math.sin(_30)+_e/2;
_2f=da.createText[this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx"](this.chart,s,x,y,"middle",_14[i],_2e.series.font,_2e.series.fontColor);
if(this.opt.htmlLabels){
this.htmlElements.push(_2f);
}
_10=end;
return false;
},this);
}else{
if(this.opt.labelStyle=="columns"){
_10=_f;
var _31=this.opt.omitLabels;
var _32=[];
_2.forEach(_13,function(_33,i){
var end=_10+_33*2*Math.PI;
if(i+1==_13.length){
end=_f+2*Math.PI;
}
var _34=(_10+end)/2;
_32.push({angle:_34,left:Math.cos(_34)<0,theme:_1b[i],index:i,omit:_31?end-_10<0.001:false});
_10=end;
});
var _35=g._base._getTextBox("a",{font:_d}).h;
this._getProperLabelRadius(_32,_35,_18.r*1.1);
_2.forEach(_32,function(_36,i){
if(!_36.omit){
var _37=_18.cx-_18.r*2,_38=_18.cx+_18.r*2,_39=g._base._getTextBox(_14[i],{font:_36.theme.series.font}).w,x=_18.cx+_36.labelR*Math.cos(_36.angle),y=_18.cy+_36.labelR*Math.sin(_36.angle),_3a=(_36.left)?(_37+_39):(_38-_39),_3b=(_36.left)?_37:_3a;
var _3c=s.createPath().moveTo(_18.cx+_18.r*Math.cos(_36.angle),_18.cy+_18.r*Math.sin(_36.angle));
if(Math.abs(_36.labelR*Math.cos(_36.angle))<_18.r*2-_39){
_3c.lineTo(x,y);
}
_3c.lineTo(_3a,y).setStroke(_36.theme.series.labelWiring);
var _3d=da.createText[this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx"](this.chart,s,_3b,y,"left",_14[i],_36.theme.series.font,_36.theme.series.fontColor);
if(this.opt.htmlLabels){
this.htmlElements.push(_3d);
}
}
},this);
}
}
}
var esi=0;
this._eventSeries[this.run.name]=df.map(run,function(v){
return v<=0?null:_1f[esi++];
});
return this;
},_getProperLabelRadius:function(_3e,_3f,_40){
var _41,_42,_43=1,_44=1;
if(_3e.length==1){
_3e[0].labelR=_40;
return;
}
for(var i=0;i<_3e.length;i++){
var _45=Math.abs(Math.sin(_3e[i].angle));
if(_3e[i].left){
if(_43>=_45){
_43=_45;
_41=_3e[i];
}
}else{
if(_44>=_45){
_44=_45;
_42=_3e[i];
}
}
}
_41.labelR=_42.labelR=_40;
this._calculateLabelR(_41,_3e,_3f);
this._calculateLabelR(_42,_3e,_3f);
},_calculateLabelR:function(_46,_47,_48){
var i=_46.index,_49=_47.length,_4a=_46.labelR,_4b;
while(!(_47[i%_49].left^_47[(i+1)%_49].left)){
if(!_47[(i+1)%_49].omit){
_4b=(Math.sin(_47[i%_49].angle)*_4a+((_47[i%_49].left)?(-_48):_48))/Math.sin(_47[(i+1)%_49].angle);
_4a=(_4b<_46.labelR)?_46.labelR:_4b;
_47[(i+1)%_49].labelR=_4a;
}
i++;
}
i=_46.index;
var j=(i==0)?_49-1:i-1;
while(!(_47[i].left^_47[j].left)){
if(!_47[j].omit){
_4b=(Math.sin(_47[i].angle)*_4a+((_47[i].left)?_48:(-_48)))/Math.sin(_47[j].angle);
_4a=(_4b<_46.labelR)?_46.labelR:_4b;
_47[j].labelR=_4a;
}
i--;
j--;
i=(i<0)?i+_47.length:i;
j=(j<0)?j+_47.length:j;
}
},_getLabel:function(_4c){
return dc.getLabel(_4c,this.opt.fixed,this.opt.precision);
}});
});
