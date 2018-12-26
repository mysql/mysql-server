//>>built
define("dojox/charting/plot2d/Spider",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/html","dojo/_base/array","dojo/dom-geometry","dojo/_base/fx","dojo/fx","dojo/_base/sniff","../Element","./_PlotEvents","dojo/_base/Color","dojox/color/_base","./common","../axis2d/common","../scaler/primitive","dojox/gfx","dojox/gfx/matrix","dojox/gfx/fx","dojox/lang/functional","dojox/lang/utils","dojo/fx/easing"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,dc,da,_e,g,m,_f,df,du,_10){
var _11=0.2;
var _12=_2("dojox.charting.plot2d.Spider",[_a,_b],{defaultParams:{labels:true,ticks:false,fixed:true,precision:1,labelOffset:-10,labelStyle:"default",htmlLabels:true,startAngle:-90,divisions:3,axisColor:"",axisWidth:0,spiderColor:"",spiderWidth:0,seriesWidth:0,seriesFillAlpha:0.2,spiderOrigin:0.16,markerSize:3,spiderType:"polygon",animationType:_10.backOut,axisTickFont:"",axisTickFontColor:"",axisFont:"",axisFontColor:""},optionalParams:{radius:0,font:"",fontColor:""},constructor:function(_13,_14){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_14);
du.updateWithPattern(this.opt,_14,this.optionalParams);
this.series=[];
this.dyn=[];
this.datas={};
this.labelKey=[];
this.oldSeriePoints={};
this.animations={};
},clear:function(){
this.dirty=true;
this.dyn=[];
this.series=[];
this.datas={};
this.labelKey=[];
this.oldSeriePoints={};
this.animations={};
return this;
},setAxis:function(_15){
return this;
},addSeries:function(run){
var _16=false;
this.series.push(run);
for(var key in run.data){
var val=run.data[key],_17=this.datas[key];
if(_17){
_17.vlist.push(val);
_17.min=Math.min(_17.min,val);
_17.max=Math.max(_17.max,val);
}else{
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
},calculateAxes:function(dim){
this.initializeScalers(dim,this.getSeriesStats());
return this;
},getRequiredColors:function(){
return this.series.length;
},initializeScalers:function(dim,_18){
if(this._hAxis){
if(!this._hAxis.initialized()){
this._hAxis.calculate(_18.hmin,_18.hmax,dim.width);
}
this._hScaler=this._hAxis.getScaler();
}else{
this._hScaler=_e.buildScaler(_18.hmin,_18.hmax,dim.width);
}
if(this._vAxis){
if(!this._vAxis.initialized()){
this._vAxis.calculate(_18.vmin,_18.vmax,dim.height);
}
this._vScaler=this._vAxis.getScaler();
}else{
this._vScaler=_e.buildScaler(_18.vmin,_18.vmax,dim.height);
}
return this;
},render:function(dim,_19){
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
var o=this.opt,ta=t.axis,rx=(dim.width-_19.l-_19.r)/2,ry=(dim.height-_19.t-_19.b)/2,r=Math.min(rx,ry),_1a=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font)||"normal normal normal 7pt Tahoma",_1b=o.axisFont||(ta.tick&&ta.tick.titleFont)||"normal normal normal 11pt Tahoma",_1c=o.axisTickFontColor||(ta.majorTick&&ta.majorTick.fontColor)||(ta.tick&&ta.tick.fontColor)||"silver",_1d=o.axisFontColor||(ta.tick&&ta.tick.titleFontColor)||"black",_1e=o.axisColor||(ta.tick&&ta.tick.axisColor)||"silver",_1f=o.spiderColor||(ta.tick&&ta.tick.spiderColor)||"silver",_20=o.axisWidth||(ta.stroke&&ta.stroke.width)||2,_21=o.spiderWidth||(ta.stroke&&ta.stroke.width)||2,_22=o.seriesWidth||(ta.stroke&&ta.stroke.width)||2,_23=g.normalizedLength(g.splitFontString(_1b).size),_24=m._degToRad(o.startAngle),_25=_24,_26,_27,_28,_29,_2a,_2b,_2c,_2d,_2e,_2f,_30,ro=o.spiderOrigin,dv=o.divisions>=3?o.divisions:3,ms=o.markerSize,spt=o.spiderType,at=o.animationType,_31=o.labelOffset<-10?o.labelOffset:-10,_32=0.2;
if(o.labels){
_29=_5.map(this.series,function(s){
return s.name;
},this);
_2a=df.foldl1(df.map(_29,function(_33,i){
var _34=t.series.font;
return g._base._getTextBox(_33,{font:_34}).w;
},this),"Math.max(a, b)")/2;
r=Math.min(rx-2*_2a,ry-_23)+_31;
_2b=r-_31;
}
if("radius" in o){
r=o.radius;
_2b=r-_31;
}
r/=(1+_32);
var _35={cx:_19.l+rx,cy:_19.t+ry,r:r};
for(var i=this.series.length-1;i>=0;i--){
var _36=this.series[i];
if(!this.dirty&&!_36.dirty){
t.skip();
continue;
}
_36.cleanGroup();
var run=_36.data;
if(run!==null){
var len=this._getObjectLength(run);
if(!_2c||_2c.length<=0){
_2c=[],_2d=[],_30=[];
this._buildPoints(_2c,len,_35,r,_25,true);
this._buildPoints(_2d,len,_35,r*ro,_25,true);
this._buildPoints(_30,len,_35,_2b,_25);
if(dv>2){
_2e=[],_2f=[];
for(var j=0;j<dv-2;j++){
_2e[j]=[];
this._buildPoints(_2e[j],len,_35,r*(ro+(1-ro)*(j+1)/(dv-1)),_25,true);
_2f[j]=r*(ro+(1-ro)*(j+1)/(dv-1));
}
}
}
}
}
var _37=s.createGroup(),_38={color:_1e,width:_20},_39={color:_1f,width:_21};
for(var j=_2c.length-1;j>=0;--j){
var _3a=_2c[j],st={x:_3a.x+(_3a.x-_35.cx)*_32,y:_3a.y+(_3a.y-_35.cy)*_32},nd={x:_3a.x+(_3a.x-_35.cx)*_32/2,y:_3a.y+(_3a.y-_35.cy)*_32/2};
_37.createLine({x1:_35.cx,y1:_35.cy,x2:st.x,y2:st.y}).setStroke(_38);
this._drawArrow(_37,st,nd,_38);
}
var _3b=s.createGroup();
for(var j=_30.length-1;j>=0;--j){
var _3a=_30[j],_3c=g._base._getTextBox(this.labelKey[j],{font:_1b}).w||0,_3d=this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx",_3e=da.createText[_3d](this.chart,_3b,(!_6.isBodyLtr()&&_3d=="html")?(_3a.x+_3c-dim.width):_3a.x,_3a.y,"middle",this.labelKey[j],_1b,_1d);
if(this.opt.htmlLabels){
this.htmlElements.push(_3e);
}
}
var _3f=s.createGroup();
if(spt=="polygon"){
_3f.createPolyline(_2c).setStroke(_39);
_3f.createPolyline(_2d).setStroke(_39);
if(_2e.length>0){
for(var j=_2e.length-1;j>=0;--j){
_3f.createPolyline(_2e[j]).setStroke(_39);
}
}
}else{
var _40=this._getObjectLength(this.datas);
_3f.createCircle({cx:_35.cx,cy:_35.cy,r:r}).setStroke(_39);
_3f.createCircle({cx:_35.cx,cy:_35.cy,r:r*ro}).setStroke(_39);
if(_2f.length>0){
for(var j=_2f.length-1;j>=0;--j){
_3f.createCircle({cx:_35.cx,cy:_35.cy,r:_2f[j]}).setStroke(_39);
}
}
}
var _41=s.createGroup(),len=this._getObjectLength(this.datas),k=0;
for(var key in this.datas){
var _42=this.datas[key],min=_42.min,max=_42.max,_43=max-min,end=_25+2*Math.PI*k/len;
for(var i=0;i<dv;i++){
var _44=min+_43*i/(dv-1),_3a=this._getCoordinate(_35,r*(ro+(1-ro)*i/(dv-1)),end);
_44=this._getLabel(_44);
var _3c=g._base._getTextBox(_44,{font:_1a}).w||0,_3d=this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx";
if(this.opt.htmlLabels){
this.htmlElements.push(da.createText[_3d](this.chart,_41,(!_6.isBodyLtr()&&_3d=="html")?(_3a.x+_3c-dim.width):_3a.x,_3a.y,"start",_44,_1a,_1c));
}
}
k++;
}
this.chart.seriesShapes={};
var _45=[];
for(var i=this.series.length-1;i>=0;i--){
var _36=this.series[i],run=_36.data;
if(run!==null){
var _46=[],k=0,_47=[];
for(var key in run){
var _42=this.datas[key],min=_42.min,max=_42.max,_43=max-min,_48=run[key],end=_25+2*Math.PI*k/len,_3a=this._getCoordinate(_35,r*(ro+(1-ro)*(_48-min)/_43),end);
_46.push(_3a);
_47.push({sname:_36.name,key:key,data:_48});
k++;
}
_46[_46.length]=_46[0];
_47[_47.length]=_47[0];
var _49=this._getBoundary(_46),_4a=t.next("spider",[o,_36]),ts=_36.group,f=g.normalizeColor(_4a.series.fill),sk={color:_4a.series.fill,width:_22};
f.a=o.seriesFillAlpha;
_36.dyn={fill:f,stroke:sk};
var _4b=this.oldSeriePoints[_36.name];
var cs=this._createSeriesEntry(ts,(_4b||_2d),_46,f,sk,r,ro,ms,at);
this.chart.seriesShapes[_36.name]=cs;
this.oldSeriePoints[_36.name]=_46;
var po={element:"spider_poly",index:i,id:"spider_poly_"+_36.name,run:_36,plot:this,shape:cs.poly,parent:ts,brect:_49,cx:_35.cx,cy:_35.cy,cr:r,f:f,s:s};
this._connectEvents(po);
var so={element:"spider_plot",index:i,id:"spider_plot_"+_36.name,run:_36,plot:this,shape:_36.group};
this._connectEvents(so);
_5.forEach(cs.circles,function(c,i){
var _4c=c.getShape(),co={element:"spider_circle",index:i,id:"spider_circle_"+_36.name+i,run:_36,plot:this,shape:c,parent:ts,tdata:_47[i],cx:_46[i].x,cy:_46[i].y,f:f,s:s};
this._connectEvents(co);
},this);
}
}
return this;
},_createSeriesEntry:function(ts,_4d,sps,f,sk,r,ro,ms,at){
var _4e=ts.createPolyline(_4d).setFill(f).setStroke(sk),_4f=[];
for(var j=0;j<_4d.length;j++){
var _50=_4d[j],cr=ms;
var _51=ts.createCircle({cx:_50.x,cy:_50.y,r:cr}).setFill(f).setStroke(sk);
_4f.push(_51);
}
var _52=_5.map(sps,function(np,j){
var sp=_4d[j],_53=new _7.Animation({duration:1000,easing:at,curve:[sp.y,np.y]});
var spl=_4e,sc=_4f[j];
_3.connect(_53,"onAnimate",function(y){
var _54=spl.getShape();
_54.points[j].y=y;
spl.setShape(_54);
var _55=sc.getShape();
_55.cy=y;
sc.setShape(_55);
});
return _53;
});
var _56=_5.map(sps,function(np,j){
var sp=_4d[j],_57=new _7.Animation({duration:1000,easing:at,curve:[sp.x,np.x]});
var spl=_4e,sc=_4f[j];
_3.connect(_57,"onAnimate",function(x){
var _58=spl.getShape();
_58.points[j].x=x;
spl.setShape(_58);
var _59=sc.getShape();
_59.cx=x;
sc.setShape(_59);
});
return _57;
});
var _5a=_8.combine(_52.concat(_56));
_5a.play();
return {group:ts,poly:_4e,circles:_4f};
},plotEvent:function(o){
var _5b=o.id?o.id:"default",a;
if(_5b in this.animations){
a=this.animations[_5b];
a.anim&&a.anim.stop(true);
}else{
a=this.animations[_5b]={};
}
if(o.element=="spider_poly"){
if(!a.color){
var _5c=o.shape.getFill();
if(!_5c||!(_5c instanceof _c)){
return;
}
a.color={start:_5c,end:_5d(_5c)};
}
var _5e=a.color.start,end=a.color.end;
if(o.type=="onmouseout"){
var t=_5e;
_5e=end;
end=t;
}
a.anim=_f.animateFill({shape:o.shape,duration:800,easing:_10.backOut,color:{start:_5e,end:end}});
a.anim.play();
}else{
if(o.element=="spider_circle"){
var _5f,_60,_61=1.5;
if(o.type=="onmouseover"){
_5f=m.identity;
_60=_61;
var _62={type:"rect"};
_62.x=o.cx;
_62.y=o.cy;
_62.width=_62.height=1;
var lt=_4.coords(this.chart.node,true);
_62.x+=lt.x;
_62.y+=lt.y;
_62.x=Math.round(_62.x);
_62.y=Math.round(_62.y);
_62.width=Math.ceil(_62.width);
_62.height=Math.ceil(_62.height);
this.aroundRect=_62;
var _63=["after","before"];
dc.doIfLoaded("dijit/Tooltip",dojo.hitch(this,function(_64){
_64.show(o.tdata.sname+"<br/>"+o.tdata.key+"<br/>"+o.tdata.data,this.aroundRect,_63);
}));
}else{
_5f=m.scaleAt(_61,o.cx,o.cy);
_60=1/_61;
dc.doIfLoaded("dijit/Tooltip",dojo.hitch(this,function(_65){
this.aroundRect&&_65.hide(this.aroundRect);
}));
}
var cs=o.shape.getShape(),_5f=m.scaleAt(_61,cs.cx,cs.cy),_66={shape:o.shape,duration:200,easing:_10.backOut,transform:[{name:"scaleAt",start:[1,cs.cx,cs.cy],end:[_60,cs.cx,cs.cy]},_5f]};
a.anim=_f.animateTransform(_66);
a.anim.play();
}else{
if(o.element=="spider_plot"){
if(o.type=="onmouseover"&&!_9("ie")){
o.shape.moveToFront();
}
}
}
}
},_getBoundary:function(_67){
var _68=_67[0].x,_69=_67[0].x,_6a=_67[0].y,_6b=_67[0].y;
for(var i=0;i<_67.length;i++){
var _6c=_67[i];
_68=Math.max(_6c.x,_68);
_6a=Math.max(_6c.y,_6a);
_69=Math.min(_6c.x,_69);
_6b=Math.min(_6c.y,_6b);
}
return {x:_69,y:_6b,width:_68-_69,height:_6a-_6b};
},_drawArrow:function(s,_6d,end,_6e){
var len=Math.sqrt(Math.pow(end.x-_6d.x,2)+Math.pow(end.y-_6d.y,2)),sin=(end.y-_6d.y)/len,cos=(end.x-_6d.x)/len,_6f={x:end.x+(len/3)*(-sin),y:end.y+(len/3)*cos},_70={x:end.x+(len/3)*sin,y:end.y+(len/3)*(-cos)};
s.createPolyline([_6d,_6f,_70]).setFill(_6e.color).setStroke(_6e);
},_buildPoints:function(_71,_72,_73,_74,_75,_76){
for(var i=0;i<_72;i++){
var end=_75+2*Math.PI*i/_72;
_71.push(this._getCoordinate(_73,_74,end));
}
if(_76){
_71.push(this._getCoordinate(_73,_74,_75+2*Math.PI));
}
},_getCoordinate:function(_77,_78,_79){
return {x:_77.cx+_78*Math.cos(_79),y:_77.cy+_78*Math.sin(_79)};
},_getObjectLength:function(obj){
var _7a=0;
if(_1.isObject(obj)){
for(var key in obj){
_7a++;
}
}
return _7a;
},_getLabel:function(_7b){
return dc.getLabel(_7b,this.opt.fixed,this.opt.precision);
}});
function _5d(_7c){
var a=new _d.Color(_7c),x=a.toHsl();
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
var _7c=_d.fromHsl(x);
_7c.a=0.7;
return _7c;
};
return _12;
});
