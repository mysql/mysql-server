//>>built
define("dojox/charting/plot2d/Spider",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/dom-geometry","dojo/_base/fx","dojo/fx","dojo/_base/sniff","./Base","./_PlotEvents","dojo/_base/Color","dojox/color/_base","./common","../axis2d/common","../scaler/primitive","dojox/gfx","dojox/gfx/matrix","dojox/gfx/fx","dojox/lang/functional","dojox/lang/utils","dojo/fx/easing"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,dc,da,_d,g,m,_e,df,du,_f){
var _10=0.2;
var _11=_2("dojox.charting.plot2d.Spider",[_9,_a],{defaultParams:{labels:true,ticks:false,fixed:true,precision:1,labelOffset:-10,labelStyle:"default",htmlLabels:true,startAngle:-90,divisions:3,axisColor:"",axisWidth:0,spiderColor:"",spiderWidth:0,seriesWidth:0,seriesFillAlpha:0.2,spiderOrigin:0.16,markerSize:3,spiderType:"polygon",animationType:_f.backOut,axisTickFont:"",axisTickFontColor:"",axisFont:"",axisFontColor:""},optionalParams:{radius:0,font:"",fontColor:""},constructor:function(_12,_13){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_13);
du.updateWithPattern(this.opt,_13,this.optionalParams);
this.dyn=[];
this.datas={};
this.labelKey=[];
this.oldSeriePoints={};
this.animations={};
},clear:function(){
this.inherited(arguments);
this.dyn=[];
this.axes=[];
this.datas={};
this.labelKey=[];
this.oldSeriePoints={};
this.animations={};
return this;
},setAxis:function(_14){
if(_14){
if(_14.opt.min!=undefined){
this.datas[_14.name].min=_14.opt.min;
}
if(_14.opt.max!=undefined){
this.datas[_14.name].max=_14.opt.max;
}
}
return this;
},addSeries:function(run){
var _15=false;
this.series.push(run);
for(var key in run.data){
var val=run.data[key],_16=this.datas[key];
if(_16){
_16.vlist.push(val);
_16.min=Math.min(_16.min,val);
_16.max=Math.max(_16.max,val);
}else{
var _17="__"+key;
this.axes.push(_17);
this[_17]=key;
this.datas[key]={min:val,max:val,vlist:[val]};
}
}
if(this.labelKey.length<=0){
for(var key in run.data){
this.labelKey.push(key);
}
}
return this;
},getSeriesStats:function(){
return dc.collectSimpleStats(this.series);
},render:function(dim,_18){
if(!this.dirty){
return this;
}
this.dirty=false;
this.cleanGroup();
var s=this.group,t=this.chart.theme;
this.resetEvents();
if(!this.series||!this.series.length){
return this;
}
var o=this.opt,ta=t.axis,rx=(dim.width-_18.l-_18.r)/2,ry=(dim.height-_18.t-_18.b)/2,r=Math.min(rx,ry),_19=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font)||"normal normal normal 7pt Tahoma",_1a=o.axisFont||(ta.tick&&ta.tick.titleFont)||"normal normal normal 11pt Tahoma",_1b=o.axisTickFontColor||(ta.majorTick&&ta.majorTick.fontColor)||(ta.tick&&ta.tick.fontColor)||"silver",_1c=o.axisFontColor||(ta.tick&&ta.tick.titleFontColor)||"black",_1d=o.axisColor||(ta.tick&&ta.tick.axisColor)||"silver",_1e=o.spiderColor||(ta.tick&&ta.tick.spiderColor)||"silver",_1f=o.axisWidth||(ta.stroke&&ta.stroke.width)||2,_20=o.spiderWidth||(ta.stroke&&ta.stroke.width)||2,_21=o.seriesWidth||(ta.stroke&&ta.stroke.width)||2,_22=g.normalizedLength(g.splitFontString(_1a).size),_23=m._degToRad(o.startAngle),_24=_23,_25,_26,_27,_28,_29,_2a,_2b,_2c,_2d,_2e,_2f,ro=o.spiderOrigin,dv=o.divisions>=3?o.divisions:3,ms=o.markerSize,spt=o.spiderType,at=o.animationType,_30=o.labelOffset<-10?o.labelOffset:-10,_31=0.2;
if(o.labels){
_28=_4.map(this.series,function(s){
return s.name;
},this);
_29=df.foldl1(df.map(_28,function(_32,i){
var _33=t.series.font;
return g._base._getTextBox(_32,{font:_33}).w;
},this),"Math.max(a, b)")/2;
r=Math.min(rx-2*_29,ry-_22)+_30;
_2a=r-_30;
}
if("radius" in o){
r=o.radius;
_2a=r-_30;
}
r/=(1+_31);
var _34={cx:_18.l+rx,cy:_18.t+ry,r:r};
for(var i=this.series.length-1;i>=0;i--){
var _35=this.series[i];
if(!this.dirty&&!_35.dirty){
t.skip();
continue;
}
_35.cleanGroup();
var run=_35.data;
if(run!==null){
var len=this._getObjectLength(run);
if(!_2b||_2b.length<=0){
_2b=[],_2c=[],_2f=[];
this._buildPoints(_2b,len,_34,r,_24,true);
this._buildPoints(_2c,len,_34,r*ro,_24,true);
this._buildPoints(_2f,len,_34,_2a,_24);
if(dv>2){
_2d=[],_2e=[];
for(var j=0;j<dv-2;j++){
_2d[j]=[];
this._buildPoints(_2d[j],len,_34,r*(ro+(1-ro)*(j+1)/(dv-1)),_24,true);
_2e[j]=r*(ro+(1-ro)*(j+1)/(dv-1));
}
}
}
}
}
var _36=s.createGroup(),_37={color:_1d,width:_1f},_38={color:_1e,width:_20};
for(var j=_2b.length-1;j>=0;--j){
var _39=_2b[j],st={x:_39.x+(_39.x-_34.cx)*_31,y:_39.y+(_39.y-_34.cy)*_31},nd={x:_39.x+(_39.x-_34.cx)*_31/2,y:_39.y+(_39.y-_34.cy)*_31/2};
_36.createLine({x1:_34.cx,y1:_34.cy,x2:st.x,y2:st.y}).setStroke(_37);
this._drawArrow(_36,st,nd,_37);
}
var _3a=s.createGroup();
for(var j=_2f.length-1;j>=0;--j){
var _39=_2f[j],_3b=g._base._getTextBox(this.labelKey[j],{font:_1a}).w||0,_3c=this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx",_3d=da.createText[_3c](this.chart,_3a,(!_5.isBodyLtr()&&_3c=="html")?(_39.x+_3b-dim.width):_39.x,_39.y,"middle",this.labelKey[j],_1a,_1c);
if(this.opt.htmlLabels){
this.htmlElements.push(_3d);
}
}
var _3e=s.createGroup();
if(spt=="polygon"){
_3e.createPolyline(_2b).setStroke(_38);
_3e.createPolyline(_2c).setStroke(_38);
if(_2d.length>0){
for(var j=_2d.length-1;j>=0;--j){
_3e.createPolyline(_2d[j]).setStroke(_38);
}
}
}else{
var _3f=this._getObjectLength(this.datas);
_3e.createCircle({cx:_34.cx,cy:_34.cy,r:r}).setStroke(_38);
_3e.createCircle({cx:_34.cx,cy:_34.cy,r:r*ro}).setStroke(_38);
if(_2e.length>0){
for(var j=_2e.length-1;j>=0;--j){
_3e.createCircle({cx:_34.cx,cy:_34.cy,r:_2e[j]}).setStroke(_38);
}
}
}
var _40=s.createGroup(),len=this._getObjectLength(this.datas),k=0;
for(var key in this.datas){
var _41=this.datas[key],min=_41.min,max=_41.max,_42=max-min,end=_24+2*Math.PI*k/len;
for(var i=0;i<dv;i++){
var _43=min+_42*i/(dv-1),_39=this._getCoordinate(_34,r*(ro+(1-ro)*i/(dv-1)),end);
_43=this._getLabel(_43);
var _3b=g._base._getTextBox(_43,{font:_19}).w||0,_3c=this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx";
if(this.opt.htmlLabels){
this.htmlElements.push(da.createText[_3c](this.chart,_40,(!_5.isBodyLtr()&&_3c=="html")?(_39.x+_3b-dim.width):_39.x,_39.y,"start",_43,_19,_1b));
}
}
k++;
}
this.chart.seriesShapes={};
var _44=[];
for(var i=this.series.length-1;i>=0;i--){
var _35=this.series[i],run=_35.data;
if(run!==null){
var _45=[],k=0,_46=[];
for(var key in run){
var _41=this.datas[key],min=_41.min,max=_41.max,_42=max-min,_47=run[key],end=_24+2*Math.PI*k/len,_39=this._getCoordinate(_34,r*(ro+(1-ro)*(_47-min)/_42),end);
_45.push(_39);
_46.push({sname:_35.name,key:key,data:_47});
k++;
}
_45[_45.length]=_45[0];
_46[_46.length]=_46[0];
var _48=this._getBoundary(_45),_49=t.next("spider",[o,_35]),ts=_35.group,f=g.normalizeColor(_49.series.fill),sk={color:_49.series.fill,width:_21};
f.a=o.seriesFillAlpha;
_35.dyn={fill:f,stroke:sk};
var _4a=this.oldSeriePoints[_35.name];
var cs=this._createSeriesEntry(ts,(_4a||_2c),_45,f,sk,r,ro,ms,at);
this.chart.seriesShapes[_35.name]=cs;
this.oldSeriePoints[_35.name]=_45;
var po={element:"spider_poly",index:i,id:"spider_poly_"+_35.name,run:_35,plot:this,shape:cs.poly,parent:ts,brect:_48,cx:_34.cx,cy:_34.cy,cr:r,f:f,s:s};
this._connectEvents(po);
var so={element:"spider_plot",index:i,id:"spider_plot_"+_35.name,run:_35,plot:this,shape:_35.group};
this._connectEvents(so);
_4.forEach(cs.circles,function(c,i){
var _4b=c.getShape(),co={element:"spider_circle",index:i,id:"spider_circle_"+_35.name+i,run:_35,plot:this,shape:c,parent:ts,tdata:_46[i],cx:_45[i].x,cy:_45[i].y,f:f,s:s};
this._connectEvents(co);
},this);
}
}
return this;
},_createSeriesEntry:function(ts,_4c,sps,f,sk,r,ro,ms,at){
var _4d=ts.createPolyline(_4c).setFill(f).setStroke(sk),_4e=[];
for(var j=0;j<_4c.length;j++){
var _4f=_4c[j],cr=ms;
var _50=ts.createCircle({cx:_4f.x,cy:_4f.y,r:cr}).setFill(f).setStroke(sk);
_4e.push(_50);
}
var _51=_4.map(sps,function(np,j){
var sp=_4c[j],_52=new _6.Animation({duration:1000,easing:at,curve:[sp.y,np.y]});
var spl=_4d,sc=_4e[j];
_3.connect(_52,"onAnimate",function(y){
var _53=spl.getShape();
_53.points[j].y=y;
spl.setShape(_53);
var _54=sc.getShape();
_54.cy=y;
sc.setShape(_54);
});
return _52;
});
var _55=_4.map(sps,function(np,j){
var sp=_4c[j],_56=new _6.Animation({duration:1000,easing:at,curve:[sp.x,np.x]});
var spl=_4d,sc=_4e[j];
_3.connect(_56,"onAnimate",function(x){
var _57=spl.getShape();
_57.points[j].x=x;
spl.setShape(_57);
var _58=sc.getShape();
_58.cx=x;
sc.setShape(_58);
});
return _56;
});
var _59=_7.combine(_51.concat(_55));
_59.play();
return {group:ts,poly:_4d,circles:_4e};
},plotEvent:function(o){
var _5a=o.id?o.id:"default",a;
if(_5a in this.animations){
a=this.animations[_5a];
a.anim&&a.anim.stop(true);
}else{
a=this.animations[_5a]={};
}
if(o.element=="spider_poly"){
if(!a.color){
var _5b=o.shape.getFill();
if(!_5b||!(_5b instanceof _b)){
return;
}
a.color={start:_5b,end:_5c(_5b)};
}
var _5d=a.color.start,end=a.color.end;
if(o.type=="onmouseout"){
var t=_5d;
_5d=end;
end=t;
}
a.anim=_e.animateFill({shape:o.shape,duration:800,easing:_f.backOut,color:{start:_5d,end:end}});
a.anim.play();
}else{
if(o.element=="spider_circle"){
var _5e,_5f,_60=1.5;
if(o.type=="onmouseover"){
_5e=m.identity;
_5f=_60;
var _61={type:"rect"};
_61.x=o.cx;
_61.y=o.cy;
_61.w=_61.h=1;
var lt=this.chart.getCoords();
_61.x+=lt.x;
_61.y+=lt.y;
_61.x=Math.round(_61.x);
_61.y=Math.round(_61.y);
this.aroundRect=_61;
var _62=["after-centered","before-centered"];
dc.doIfLoaded("dijit/Tooltip",_1.hitch(this,function(_63){
_63.show(o.tdata.sname+"<br/>"+o.tdata.key+"<br/>"+o.tdata.data,this.aroundRect,_62);
}));
}else{
_5e=m.scaleAt(_60,o.cx,o.cy);
_5f=1/_60;
dc.doIfLoaded("dijit/Tooltip",_1.hitch(this,function(_64){
this.aroundRect&&_64.hide(this.aroundRect);
}));
}
var cs=o.shape.getShape(),_5e=m.scaleAt(_60,cs.cx,cs.cy),_65={shape:o.shape,duration:200,easing:_f.backOut,transform:[{name:"scaleAt",start:[1,cs.cx,cs.cy],end:[_5f,cs.cx,cs.cy]},_5e]};
a.anim=_e.animateTransform(_65);
a.anim.play();
}else{
if(o.element=="spider_plot"){
if(o.type=="onmouseover"&&!_8("ie")){
o.shape.moveToFront();
}
}
}
}
},_getBoundary:function(_66){
var _67=_66[0].x,_68=_66[0].x,_69=_66[0].y,_6a=_66[0].y;
for(var i=0;i<_66.length;i++){
var _6b=_66[i];
_67=Math.max(_6b.x,_67);
_69=Math.max(_6b.y,_69);
_68=Math.min(_6b.x,_68);
_6a=Math.min(_6b.y,_6a);
}
return {x:_68,y:_6a,width:_67-_68,height:_69-_6a};
},_drawArrow:function(s,_6c,end,_6d){
var len=Math.sqrt(Math.pow(end.x-_6c.x,2)+Math.pow(end.y-_6c.y,2)),sin=(end.y-_6c.y)/len,cos=(end.x-_6c.x)/len,_6e={x:end.x+(len/3)*(-sin),y:end.y+(len/3)*cos},_6f={x:end.x+(len/3)*sin,y:end.y+(len/3)*(-cos)};
s.createPolyline([_6c,_6e,_6f]).setFill(_6d.color).setStroke(_6d);
},_buildPoints:function(_70,_71,_72,_73,_74,_75){
for(var i=0;i<_71;i++){
var end=_74+2*Math.PI*i/_71;
_70.push(this._getCoordinate(_72,_73,end));
}
if(_75){
_70.push(this._getCoordinate(_72,_73,_74+2*Math.PI));
}
},_getCoordinate:function(_76,_77,_78){
return {x:_76.cx+_77*Math.cos(_78),y:_76.cy+_77*Math.sin(_78)};
},_getObjectLength:function(obj){
var _79=0;
if(_1.isObject(obj)){
for(var key in obj){
_79++;
}
}
return _79;
},_getLabel:function(_7a){
return dc.getLabel(_7a,this.opt.fixed,this.opt.precision);
}});
function _5c(_7b){
var a=new _c.Color(_7b),x=a.toHsl();
if(x.s==0){
x.l=x.l<50?100:0;
}else{
x.s=100;
if(x.l<50){
x.l=75;
}else{
if(x.l>75){
x.l=50;
}else{
x.l=x.l-50>75-x.l?50:75;
}
}
}
var _7b=_c.fromHsl(x);
_7b.a=0.7;
return _7b;
};
return _11;
});
