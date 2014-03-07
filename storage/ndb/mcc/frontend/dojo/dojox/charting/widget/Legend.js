//>>built
define("dojox/charting/widget/Legend",["dojo/_base/lang","dojo/_base/html","dojo/_base/declare","dijit/_Widget","dojox/gfx","dojo/_base/array","dojox/lang/functional","dojox/lang/functional/array","dojox/lang/functional/fold","dojo/dom","dojo/dom-construct","dojo/dom-class","dijit/_base/manager"],function(_1,_2,_3,_4,_5,_6,df,_7,_8,_9,_a,_b,_c){
var _d=/\.(StackedColumns|StackedAreas|ClusteredBars)$/;
return _3("dojox.charting.widget.Legend",_4,{chartRef:"",horizontal:true,swatchSize:18,legendBody:null,postCreate:function(){
if(!this.chart){
if(!this.chartRef){
return;
}
this.chart=_c.byId(this.chartRef);
if(!this.chart){
var _e=_9.byId(this.chartRef);
if(_e){
this.chart=_c.byNode(_e);
}else{
return;
}
}
this.series=this.chart.chart.series;
}else{
this.series=this.chart.series;
}
this.refresh();
},buildRendering:function(){
this.domNode=_a.create("table",{role:"group","aria-label":"chart legend","class":"dojoxLegendNode"});
this.legendBody=_a.create("tbody",null,this.domNode);
this.inherited(arguments);
},refresh:function(){
if(this._surfaces){
_6.forEach(this._surfaces,function(_f){
_f.destroy();
});
}
this._surfaces=[];
while(this.legendBody.lastChild){
_a.destroy(this.legendBody.lastChild);
}
if(this.horizontal){
_b.add(this.domNode,"dojoxLegendHorizontal");
this._tr=_a.create("tr",null,this.legendBody);
this._inrow=0;
}
var s=this.series;
if(s.length==0){
return;
}
if(s[0].chart.stack[0].declaredClass=="dojox.charting.plot2d.Pie"){
var t=s[0].chart.stack[0];
if(typeof t.run.data[0]=="number"){
var _10=df.map(t.run.data,"Math.max(x, 0)");
if(df.every(_10,"<= 0")){
return;
}
var _11=df.map(_10,"/this",df.foldl(_10,"+",0));
_6.forEach(_11,function(x,i){
this._addLabel(t.dyn[i],t._getLabel(x*100)+"%");
},this);
}else{
_6.forEach(t.run.data,function(x,i){
this._addLabel(t.dyn[i],x.legend||x.text||x.y);
},this);
}
}else{
if(this._isReversal()){
s=s.slice(0).reverse();
}
_6.forEach(s,function(x){
this._addLabel(x.dyn,x.legend||x.name);
},this);
}
},_addLabel:function(dyn,_12){
var _13=_a.create("td"),_14=_a.create("div",null,_13),_15=_a.create("label",null,_13),div=_a.create("div",{style:{"width":this.swatchSize+"px","height":this.swatchSize+"px","float":"left"}},_14);
_b.add(_14,"dojoxLegendIcon dijitInline");
_b.add(_15,"dojoxLegendText");
if(this._tr){
this._tr.appendChild(_13);
if(++this._inrow===this.horizontal){
this._tr=_a.create("tr",null,this.legendBody);
this._inrow=0;
}
}else{
var tr=_a.create("tr",null,this.legendBody);
tr.appendChild(_13);
}
this._makeIcon(div,dyn);
_15.innerHTML=String(_12);
_15.dir=this.getTextDir(_12,_15.dir);
},_makeIcon:function(div,dyn){
var mb={h:this.swatchSize,w:this.swatchSize};
var _16=_5.createSurface(div,mb.w,mb.h);
this._surfaces.push(_16);
if(dyn.fill){
_16.createRect({x:2,y:2,width:mb.w-4,height:mb.h-4}).setFill(dyn.fill).setStroke(dyn.stroke);
}else{
if(dyn.stroke||dyn.marker){
var _17={x1:0,y1:mb.h/2,x2:mb.w,y2:mb.h/2};
if(dyn.stroke){
_16.createLine(_17).setStroke(dyn.stroke);
}
if(dyn.marker){
var c={x:mb.w/2,y:mb.h/2};
if(dyn.stroke){
_16.createPath({path:"M"+c.x+" "+c.y+" "+dyn.marker}).setFill(dyn.stroke.color).setStroke(dyn.stroke);
}else{
_16.createPath({path:"M"+c.x+" "+c.y+" "+dyn.marker}).setFill(dyn.color).setStroke(dyn.color);
}
}
}else{
_16.createRect({x:2,y:2,width:mb.w-4,height:mb.h-4}).setStroke("black");
_16.createLine({x1:2,y1:2,x2:mb.w-2,y2:mb.h-2}).setStroke("black");
_16.createLine({x1:2,y1:mb.h-2,x2:mb.w-2,y2:2}).setStroke("black");
}
}
},_isReversal:function(){
return (!this.horizontal)&&_6.some(this.chart.stack,function(_18){
return _d.test(_18.declaredClass);
});
}});
});
