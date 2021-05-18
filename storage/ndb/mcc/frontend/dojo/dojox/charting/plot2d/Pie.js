//>>built
define("dojox/charting/plot2d/Pie",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/dom-geometry","dojo/_base/Color","./Base","./_PlotEvents","./common","dojox/gfx","dojox/gfx/matrix","dojox/lang/functional","dojox/lang/utils","dojo/has"],function(_1,_2,_3,_4,_5,_6,_7,dc,g,m,df,du,_8){
var _9=0.2;
return _3("dojox.charting.plot2d.Pie",[_6,_7],{defaultParams:{labels:true,ticks:false,fixed:true,precision:1,labelOffset:20,labelStyle:"default",htmlLabels:true,radGrad:"native",fanSize:5,startAngle:0,innerRadius:0,minWidth:0,zeroDataMessage:""},optionalParams:{radius:0,omitLabels:false,stroke:{},outline:{},shadow:{},fill:{},filter:{},styleFunc:null,font:"",fontColor:"",labelWiring:{}},constructor:function(_a,_b){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_b);
du.updateWithPattern(this.opt,_b,this.optionalParams);
this.axes=[];
this.run=null;
this.dyn=[];
this.runFilter=[];
if(_b&&_b.hasOwnProperty("innerRadius")){
this._plotSetInnerRadius=true;
}
},clear:function(){
this.inherited(arguments);
this.dyn=[];
this.run=null;
return this;
},setAxis:function(_c){
return this;
},addSeries:function(_d){
this.run=_d;
return this;
},getSeriesStats:function(){
return _1.delegate(dc.defaultStats);
},getRequiredColors:function(){
return this.run?this.run.data.length:0;
},render:function(_e,_f){
if(!this.dirty){
return this;
}
this.resetEvents();
this.dirty=false;
this._eventSeries={};
this.cleanGroup();
var s=this.group,t=this.chart.theme;
if(!this._plotSetInnerRadius&&t&&t.pieInnerRadius){
this.opt.innerRadius=t.pieInnerRadius;
}
var rx=(_e.width-_f.l-_f.r)/2,ry=(_e.height-_f.t-_f.b)/2,r=Math.min(rx,ry),_10="font" in this.opt?this.opt.font:t.axis.tick.titleFont||"",_11=_10?g.normalizedLength(g.splitFontString(_10).size):0,_12=this.opt.hasOwnProperty("fontColor")?this.opt.fontColor:t.axis.tick.fontColor,_13=m._degToRad(this.opt.startAngle),_14=_13,_15,_16,_17,_18,_19,run=this.run.data,_1a=this.events();
var _1b=_1.hitch(this,function(){
var ct=t.clone();
var _1c=df.map(run,function(v){
var _1d=[this.opt,this.run];
if(v!==null&&typeof v!="number"){
_1d.push(v);
}
if(this.opt.styleFunc){
_1d.push(this.opt.styleFunc(v));
}
return ct.next("slice",_1d,true);
},this);
if("radius" in this.opt){
r=this.opt.radius<r?this.opt.radius:r;
}
var _1e={cx:_f.l+rx,cy:_f.t+ry,r:r};
var _1f=new _5(_12);
if(this.opt.innerRadius){
_1f.a=0.1;
}
var _20=this._createRing(s,_1e).setStroke(_1f);
if(this.opt.innerRadius){
_20.setFill(_1f);
}
if(this.opt.zeroDataMessage){
this.renderLabel(s,_1e.cx,_1e.cy+_11/3,this.opt.zeroDataMessage,{series:{font:_10,fontColor:_12}},null,"middle");
}
this.dyn=[];
_2.forEach(run,function(_21,i){
this.dyn.push({fill:this._plotFill(_1c[i].series.fill,_e,_f),stroke:_1c[i].series.stroke});
},this);
});
if(!this.run&&!this.run.data.ength){
_1b();
return this;
}
if(typeof run[0]=="number"){
_15=df.map(run,"x ? Math.max(x, 0) : 0");
if(df.every(_15,"<= 0")){
_1b();
return this;
}
_16=df.map(_15,"/this",df.foldl(_15,"+",0));
if(this.opt.labels){
_17=_2.map(_16,function(x){
return x>0?this._getLabel(x*100)+"%":"";
},this);
}
}else{
_15=df.map(run,"x ? Math.max(x.y, 0) : 0");
if(!_15.length||df.every(_15,"<= 0")){
_1b();
return this;
}
_16=df.map(_15,"/this",df.foldl(_15,"+",0));
if(this.opt.labels){
_17=_2.map(_16,function(x,i){
if(x<0){
return "";
}
var v=run[i];
return v.hasOwnProperty("text")?v.text:this._getLabel(x*100)+"%";
},this);
}
}
var _22=df.map(run,function(v){
var _23=[this.opt,this.run];
if(v!==null&&typeof v!="number"){
_23.push(v);
}
if(this.opt.styleFunc){
_23.push(this.opt.styleFunc(v));
}
return t.next("slice",_23,true);
},this);
if(this.opt.labels){
_18=df.foldl1(df.map(_17,function(_24,i){
var _25=_22[i].series.font;
return g._base._getTextBox(_24,{font:_25}).w;
},this),"Math.max(a, b)")/2;
if(this.opt.labelOffset<0){
r=Math.min(rx-2*_18,ry-_11)+this.opt.labelOffset;
}
}
if(this.opt.hasOwnProperty("radius")){
r=this.opt.radius<r*0.9?this.opt.radius:r*0.9;
}
if(!this.opt.radius&&this.opt.labels&&this.opt.labelStyle=="columns"){
r=r/2;
if(rx>ry&&rx>r*2){
r*=rx/(r*2);
}
if(r>=ry*0.8){
r=ry*0.8;
}
}else{
if(r>=ry*0.9){
r=ry*0.9;
}
}
_19=r-this.opt.labelOffset;
var _26={cx:_f.l+rx,cy:_f.t+ry,r:r};
this.dyn=[];
var _27=new Array(_16.length);
var _28=[],_29=_14;
var _2a=this.opt.minWidth;
_2.forEach(_16,function(_2b,i){
if(_2b===0){
_28[i]={step:0,end:_29,start:_29,weak:false};
return;
}
var end=_29+_2b*2*Math.PI;
if(i===_16.length-1){
end=_13+2*Math.PI;
}
var _2c=end-_29,_2d=_2c*r;
_28[i]={step:_2c,start:_29,end:end,weak:_2d<_2a};
_29=end;
});
if(_2a>0){
var _2e=0,_2f=_2a/r,_30=0,i;
for(i=_28.length-1;i>=0;i--){
if(_28[i].weak){
++_2e;
_30+=_28[i].step;
_28[i].step=_2f;
}
}
var _31=_2e*_2f;
if(_31>Math.PI){
_2f=Math.PI/_2e;
for(i=0;i<_28.length;++i){
if(_28[i].weak){
_28[i].step=_2f;
}
}
_31=Math.PI;
}
if(_2e>0){
_2f=1-(_31-_30)/2/Math.PI;
for(i=0;i<_28.length;++i){
if(!_28[i].weak){
_28[i].step=_2f*_28[i].step;
}
}
}
for(i=0;i<_28.length;++i){
_28[i].start=i?_28[i].end:_29;
_28[i].end=_28[i].start+_28[i].step;
}
for(i=_28.length-1;i>=0;--i){
if(_28[i].step!==0){
_28[i].end=_29+2*Math.PI;
break;
}
}
}
_29=_14;
var o,_32;
_2.some(_16,function(_33,i){
var _34;
var v=run[i],_35=_22[i];
if(_33>=1){
_32=this._plotFill(_35.series.fill,_e,_f);
_32=this._shapeFill(_32,{x:_26.cx-_26.r,y:_26.cy-_26.r,width:2*_26.r,height:2*_26.r});
_32=this._pseudoRadialFill(_32,{x:_26.cx,y:_26.cy},_26.r);
_34=this._createRing(s,_26).setFill(_32).setStroke(_35.series.stroke);
this.dyn.push({fill:_32,stroke:_35.series.stroke});
if(_1a){
o={element:"slice",index:i,run:this.run,shape:_34,x:i,y:typeof v=="number"?v:v.y,cx:_26.cx,cy:_26.cy,cr:r};
this._connectEvents(o);
_27[i]=o;
}
var k;
for(k=i+1;k<_16.length;k++){
_35=_22[k];
this.dyn.push({fill:_35.series.fill,stroke:_35.series.stroke});
}
return true;
}
if(_28[i].step===0){
this.dyn.push({fill:_35.series.fill,stroke:_35.series.stroke});
return false;
}
var _36=_28[i].step,x1=_26.cx+r*Math.cos(_29),y1=_26.cy+r*Math.sin(_29),x2=_26.cx+r*Math.cos(_29+_36),y2=_26.cy+r*Math.sin(_29+_36);
var _37=m._degToRad(this.opt.fanSize),_38;
if(_35.series.fill&&_35.series.fill.type==="radial"&&this.opt.radGrad==="fan"&&_36>_37){
var _39=s.createGroup(),_3a=Math.ceil(_36/_37),_3b=_36/_3a;
_32=this._shapeFill(_35.series.fill,{x:_26.cx-_26.r,y:_26.cy-_26.r,width:2*_26.r,height:2*_26.r});
var j,_3c,_3d,_3e,_3f,_40,_41;
for(j=0;j<_3a;++j){
_3c=_29+(j-_9)*_3b;
_3d=_29+(j+1+_9)*_3b;
_3e=j==0?x1:_26.cx+r*Math.cos(_3c);
_3f=j==0?y1:_26.cy+r*Math.sin(_3c);
_40=j==_3a-1?x2:_26.cx+r*Math.cos(_3d);
_41=j==_3a-1?y2:_26.cy+r*Math.sin(_3d);
this._createSlice(_39,_26,r,_3e,_3f,_40,_41,_3c,_3b).setFill(this._pseudoRadialFill(_32,{x:_26.cx,y:_26.cy},r,_29+(j+0.5)*_3b,_29+(j+0.5)*_3b));
}
_38=_35.series.stroke;
this._createSlice(_39,_26,r,x1,y1,x2,y2,_29,_36).setStroke(_38);
_34=_39;
}else{
_38=_35.series.stroke;
_34=this._createSlice(s,_26,r,x1,y1,x2,y2,_29,_36).setStroke(_38);
_32=_35.series.fill;
if(_32&&_32.type==="radial"){
_32=this._shapeFill(_32,{x:_26.cx-_26.r,y:_26.cy-_26.r,width:2*_26.r,height:2*_26.r});
if(this.opt.radGrad==="linear"){
_32=this._pseudoRadialFill(_32,{x:_26.cx,y:_26.cy},r,_29,_29+_36);
}
}else{
if(_32&&_32.type==="linear"){
var _42=_1.clone(_34.getBoundingBox());
if(g.renderer==="svg"){
var pos={w:0,h:0};
try{
pos=_4.position(_34.rawNode);
}
catch(ignore){
}
if(pos.h>_42.height){
_42.height=pos.h;
}
if(pos.w>_42.width){
_42.width=pos.w;
}
}
_32=this._plotFill(_32,_e,_f);
_32=this._shapeFill(_32,_42);
}
}
_34.setFill(_32);
}
this.dyn.push({fill:_32,stroke:_35.series.stroke});
if(_1a){
o={element:"slice",index:i,run:this.run,shape:_34,x:i,y:typeof v=="number"?v:v.y,cx:_26.cx,cy:_26.cy,cr:r};
this._connectEvents(o);
_27[i]=o;
}
_29=_29+_36;
return false;
},this);
if(this.opt.labels){
var _43=_8("dojo-bidi")&&this.chart.isRightToLeft();
if(this.opt.labelStyle=="default"){
_14=_13;
_29=_14;
_2.some(_16,function(_44,i){
if(_44<=0&&!this.opt.minWidth){
return false;
}
var _45=_22[i];
if(_44>=1){
this.renderLabel(s,_26.cx,_26.cy+_11/2,_17[i],_45,this.opt.labelOffset>0);
return true;
}
var end=_14+_44*2*Math.PI;
if(i+1==_16.length){
end=_13+2*Math.PI;
}
if(this.opt.omitLabels&&end-_14<0.001){
return false;
}
var _46=_29+(_28[i].step/2),x=_26.cx+_19*Math.cos(_46),y=_26.cy+_19*Math.sin(_46)+_11/2;
this.renderLabel(s,_43?_e.width-x:x,y,_17[i],_45,this.opt.labelOffset>0);
_29+=_28[i].step;
_14=end;
return false;
},this);
}else{
if(this.opt.labelStyle=="columns"){
var _47=this.opt.omitLabels;
_14=_13;
_29=_14;
var _48=[],_49=0,k;
for(k=_16.length-1;k>=0;--k){
if(_16[k]){
++_49;
}
}
_2.forEach(_16,function(_4a,i){
var end=_14+_4a*2*Math.PI;
if(i+1==_16.length){
end=_13+2*Math.PI;
}
if(this.minWidth!==0||end-_14>=0.001){
var _4b=_29+(_28[i].step/2);
if(_49===1&&!this.opt.minWidth){
_4b=(_14+end)/2;
}
_48.push({angle:_4b,left:Math.cos(_4b)<0,theme:_22[i],index:i,omit:_47?end-_14<0.001:false});
}
_14=end;
_29+=_28[i].step;
},this);
var _4c=g._base._getTextBox("a",{font:_10,whiteSpace:"nowrap"}).h;
this._getProperLabelRadius(_48,_4c,_26.r*1.1);
var _4d=_26.cx-_26.r*2,_4e=_26.cx+_26.r*2;
_2.forEach(_48,function(_4f){
if(_4f.omit){
return;
}
var _50=_22[_4f.index],_51=0;
if(_50&&_50.axis&&_50.axis.tick&&_50.axis.tick.labelGap){
_51=_50.axis.tick.labelGap;
}
var _52=g._base._getTextBox(_17[_4f.index],{font:_50.series.font,whiteSpace:"nowrap",paddingLeft:_51+"px"}).w,x=_26.cx+_4f.labelR*Math.cos(_4f.angle),y=_26.cy+_4f.labelR*Math.sin(_4f.angle),_53=(_4f.left)?(_4d+_52):(_4e-_52),_54=(_4f.left)?_4d:_53+_51,_55=_26.r,_56=s.createPath().moveTo(_26.cx+_55*Math.cos(_4f.angle),_26.cy+_55*Math.sin(_4f.angle));
if(Math.abs(_4f.labelR*Math.cos(_4f.angle))<_26.r*2-_52){
_56.lineTo(x,y);
}
_56.lineTo(_53,y).setStroke(_4f.theme.series.labelWiring);
_56.moveToBack();
var mid=_4c/3+y;
var _57=this.renderLabel(s,_54,mid||0,_17[_4f.index],_50,false,"left");
if(_1a&&!this.opt.htmlLabels){
var _58=g._base._getTextBox(_17[_4f.index],{font:_4f.theme.series.font}).w||0,_59=g.normalizedLength(g.splitFontString(_4f.theme.series.font).size);
o={element:"labels",index:_4f.index,run:this.run,shape:_57,x:_54,y:y,label:_17[_4f.index]};
var shp=_57.getShape(),lt=_4.position(this.chart.node,true),_5a=_1.mixin({type:"rect"},{x:shp.x,y:shp.y-2*_59});
_5a.x+=lt.x;
_5a.y+=lt.y;
_5a.x=Math.round(_5a.x);
_5a.y=Math.round(_5a.y);
_5a.width=Math.ceil(_58);
_5a.height=Math.ceil(_59);
o.aroundRect=_5a;
this._connectEvents(o);
_27[_16.length+_4f.index]=o;
}
},this);
}
}
}
var esi=0;
this._eventSeries[this.run.name]=df.map(run,function(v){
return v<=0?null:_27[esi++];
});
if(_8("dojo-bidi")){
this._checkOrientation(this.group,_e,_f);
}
return this;
},_getProperLabelRadius:function(_5b,_5c,_5d){
if(_5b.length==1){
_5b[0].labelR=_5d;
return;
}
var _5e={},_5f={},_60=2,_61=2,i;
var _62;
for(i=0;i<_5b.length;++i){
_62=Math.abs(Math.sin(_5b[i].angle));
if(_5b[i].left){
if(_60>_62){
_60=_62;
_5e=_5b[i];
}
}else{
if(_61>_62){
_61=_62;
_5f=_5b[i];
}
}
}
_5e.labelR=_5f.labelR=_5d;
this._caculateLabelR(_5e,_5b,_5c);
this._caculateLabelR(_5f,_5b,_5c);
},_caculateLabelR:function(_63,_64,_65){
var i,j,k,_66=_64.length,_67=_63.labelR,_68,_69=_64[_63.index].left?-_65:_65;
for(k=0,i=_63.index,j=(i+1)%_66;k<_66&&_64[i].left===_64[j].left;++k){
_68=(Math.sin(_64[i].angle)*_67+_69)/Math.sin(_64[j].angle);
_67=Math.max(_63.labelR,_68);
_64[j].labelR=_67;
i=(i+1)%_66;
j=(j+1)%_66;
}
if(k>=_66){
_64[0].labelR=_63.labelR;
}
for(k=0,i=_63.index,j=(i||_66)-1;k<_66&&_64[i].left===_64[j].left;++k){
_68=(Math.sin(_64[i].angle)*_67-_69)/Math.sin(_64[j].angle);
_67=Math.max(_63.labelR,_68);
_64[j].labelR=_67;
i=(i||_66)-1;
j=(j||_66)-1;
}
},_createRing:function(_6a,_6b){
var r=this.opt.innerRadius;
if(r>0){
r=_6b.r*(r/100);
}else{
if(r<0){
r=-r;
}
}
if(r){
return _6a.createPath({}).setAbsoluteMode(true).moveTo(_6b.cx,_6b.cy-_6b.r).arcTo(_6b.r,_6b.r,0,false,true,_6b.cx+_6b.r,_6b.cy).arcTo(_6b.r,_6b.r,0,true,true,_6b.cx,_6b.cy-_6b.r).closePath().moveTo(_6b.cx,_6b.cy-r).arcTo(r,r,0,false,true,_6b.cx+r,_6b.cy).arcTo(r,r,0,true,true,_6b.cx,_6b.cy-r).closePath();
}
return _6a.createCircle(_6b);
},_createSlice:function(_6c,_6d,R,x1,y1,x2,y2,_6e,_6f){
var r=this.opt.innerRadius;
if(r>0){
r=_6d.r*(r/100);
}else{
if(r<0){
r=-r;
}
}
if(r){
var _70=_6d.cx+r*Math.cos(_6e),_71=_6d.cy+r*Math.sin(_6e),_72=_6d.cx+r*Math.cos(_6e+_6f),_73=_6d.cy+r*Math.sin(_6e+_6f);
return _6c.createPath({}).setAbsoluteMode(true).moveTo(_70,_71).lineTo(x1,y1).arcTo(R,R,0,_6f>Math.PI,true,x2,y2).lineTo(_72,_73).arcTo(r,r,0,_6f>Math.PI,false,_70,_71).closePath();
}
return _6c.createPath({}).setAbsoluteMode(true).moveTo(_6d.cx,_6d.cy).lineTo(x1,y1).arcTo(R,R,0,_6f>Math.PI,true,x2,y2).lineTo(_6d.cx,_6d.cy).closePath();
}});
});
