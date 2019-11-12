//>>built
define("dojox/charting/widget/Legend",["dojo/_base/lang","dojo/_base/declare","dijit/_WidgetBase","dojox/gfx","dojo/_base/array","dojox/lang/functional","dojox/lang/functional/array","dojox/lang/functional/fold","dojo/dom","dojo/dom-construct","dojo/dom-class","dijit/registry"],function(_1,_2,_3,_4,_5,df,_6,_7,_8,_9,_a,_b){
return _2("dojox.charting.widget.Legend",_3,{chartRef:"",horizontal:true,swatchSize:18,legendBody:null,postCreate:function(){
if(!this.chart&&this.chartRef){
this.chart=_b.byId(this.chartRef)||_b.byNode(_8.byId(this.chartRef));
if(!this.chart){
}
}
this.chart=this.chart.chart||this.chart;
this.refresh();
},buildRendering:function(){
this.domNode=_9.create("table",{role:"group","aria-label":"chart legend","class":"dojoxLegendNode"});
this.legendBody=_9.create("tbody",null,this.domNode);
this.inherited(arguments);
},destroy:function(){
if(this._surfaces){
_5.forEach(this._surfaces,function(_c){
_c.destroy();
});
}
this.inherited(arguments);
},refresh:function(){
if(this._surfaces){
_5.forEach(this._surfaces,function(_d){
_d.destroy();
});
}
this._surfaces=[];
while(this.legendBody.lastChild){
_9.destroy(this.legendBody.lastChild);
}
if(this.horizontal){
_a.add(this.domNode,"dojoxLegendHorizontal");
this._tr=_9.create("tr",null,this.legendBody);
this._inrow=0;
}
var s=this.series||this.chart.series;
if(s.length==0){
return;
}
if(s[0].chart.stack[0].declaredClass=="dojox.charting.plot2d.Pie"){
var t=s[0].chart.stack[0];
if(typeof t.run.data[0]=="number"){
var _e=df.map(t.run.data,"Math.max(x, 0)");
var _f=df.map(_e,"/this",df.foldl(_e,"+",0));
_5.forEach(_f,function(x,i){
this._addLabel(t.dyn[i],t._getLabel(x*100)+"%");
},this);
}else{
_5.forEach(t.run.data,function(x,i){
this._addLabel(t.dyn[i],x.legend||x.text||x.y);
},this);
}
}else{
_5.forEach(s,function(x){
this._addLabel(x.dyn,x.legend||x.name);
},this);
}
},_addLabel:function(dyn,_10){
var _11=_9.create("td"),_12=_9.create("div",null,_11),_13=_9.create("label",null,_11),div=_9.create("div",{style:{"width":this.swatchSize+"px","height":this.swatchSize+"px","float":"left"}},_12);
_a.add(_12,"dojoxLegendIcon dijitInline");
_a.add(_13,"dojoxLegendText");
if(this._tr){
this._tr.appendChild(_11);
if(++this._inrow===this.horizontal){
this._tr=_9.create("tr",null,this.legendBody);
this._inrow=0;
}
}else{
var tr=_9.create("tr",null,this.legendBody);
tr.appendChild(_11);
}
this._makeIcon(div,dyn);
_13.innerHTML=String(_10);
_13.dir=this.getTextDir(_10,_13.dir);
},_makeIcon:function(div,dyn){
var mb={h:this.swatchSize,w:this.swatchSize};
var _14=_4.createSurface(div,mb.w,mb.h);
this._surfaces.push(_14);
if(dyn.fill){
_14.createRect({x:2,y:2,width:mb.w-4,height:mb.h-4}).setFill(dyn.fill).setStroke(dyn.stroke);
}else{
if(dyn.stroke||dyn.marker){
var _15={x1:0,y1:mb.h/2,x2:mb.w,y2:mb.h/2};
if(dyn.stroke){
_14.createLine(_15).setStroke(dyn.stroke);
}
if(dyn.marker){
var c={x:mb.w/2,y:mb.h/2};
_14.createPath({path:"M"+c.x+" "+c.y+" "+dyn.marker}).setFill(dyn.markerFill).setStroke(dyn.markerStroke);
}
}else{
_14.createRect({x:2,y:2,width:mb.w-4,height:mb.h-4}).setStroke("black");
_14.createLine({x1:2,y1:2,x2:mb.w-2,y2:mb.h-2}).setStroke("black");
_14.createLine({x1:2,y1:mb.h-2,x2:mb.w-2,y2:2}).setStroke("black");
}
}
}});
});
