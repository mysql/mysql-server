//>>built
define("dojox/charting/widget/Chart",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/dom-attr","dojo/_base/declare","dojo/query","dijit/_WidgetBase","../Chart","dojox/lang/utils","dojox/lang/functional","dojox/lang/functional/lambda"],function(_1,_2,_3,_4,_5,_6,_7,_8,du,df,_9){
var _a,_b,_c,_d,_e,_f=function(o){
return o;
},dc=_2.getObject("dojox.charting");
_a=function(_10,_11,kw){
var dp=eval("("+_11+".prototype.defaultParams)");
var x,_12;
for(x in dp){
if(x in kw){
continue;
}
_12=_10.getAttribute(x);
kw[x]=du.coerceType(dp[x],_12==null||typeof _12=="undefined"?dp[x]:_12);
}
var op=eval("("+_11+".prototype.optionalParams)");
for(x in op){
if(x in kw){
continue;
}
_12=_10.getAttribute(x);
if(_12!=null){
kw[x]=du.coerceType(op[x],_12);
}
}
};
_b=function(_13){
var _14=_13.getAttribute("name"),_15=_13.getAttribute("type");
if(!_14){
return null;
}
var o={name:_14,kwArgs:{}},kw=o.kwArgs;
if(_15){
if(dc.axis2d[_15]){
_15=_1._scopeName+"x.charting.axis2d."+_15;
}
var _16=eval("("+_15+")");
if(_16){
kw.type=_16;
}
}else{
_15=_1._scopeName+"x.charting.axis2d.Default";
}
_a(_13,_15,kw);
if(kw.font||kw.fontColor){
if(!kw.tick){
kw.tick={};
}
if(kw.font){
kw.tick.font=kw.font;
}
if(kw.fontColor){
kw.tick.fontColor=kw.fontColor;
}
}
return o;
};
_c=function(_17){
var _18=_17.getAttribute("name"),_19=_17.getAttribute("type");
if(!_18){
return null;
}
var o={name:_18,kwArgs:{}},kw=o.kwArgs;
if(_19){
if(dc.plot2d&&dc.plot2d[_19]){
_19=_1._scopeName+"x.charting.plot2d."+_19;
}
var _1a=eval("("+_19+")");
if(_1a){
kw.type=_1a;
}
}else{
_19=_1._scopeName+"x.charting.plot2d.Default";
}
_a(_17,_19,kw);
return o;
};
_d=function(_1b){
var _1c=_1b.getAttribute("plot"),_1d=_1b.getAttribute("type");
if(!_1c){
_1c="default";
}
var o={plot:_1c,kwArgs:{}},kw=o.kwArgs;
if(_1d){
if(dc.action2d[_1d]){
_1d=_1._scopeName+"x.charting.action2d."+_1d;
}
var _1e=eval("("+_1d+")");
if(!_1e){
return null;
}
o.action=_1e;
}else{
return null;
}
_a(_1b,_1d,kw);
return o;
};
_e=function(_1f){
var ga=_2.partial(_4.get,_1f);
var _20=ga("name");
if(!_20){
return null;
}
var o={name:_20,kwArgs:{}},kw=o.kwArgs,t;
t=ga("plot");
if(t!=null){
kw.plot=t;
}
t=ga("marker");
if(t!=null){
kw.marker=t;
}
t=ga("stroke");
if(t!=null){
kw.stroke=eval("("+t+")");
}
t=ga("outline");
if(t!=null){
kw.outline=eval("("+t+")");
}
t=ga("shadow");
if(t!=null){
kw.shadow=eval("("+t+")");
}
t=ga("fill");
if(t!=null){
kw.fill=eval("("+t+")");
}
t=ga("font");
if(t!=null){
kw.font=t;
}
t=ga("fontColor");
if(t!=null){
kw.fontColor=eval("("+t+")");
}
t=ga("legend");
if(t!=null){
kw.legend=t;
}
t=ga("data");
if(t!=null){
o.type="data";
o.data=t?_3.map(String(t).split(","),Number):[];
return o;
}
t=ga("array");
if(t!=null){
o.type="data";
o.data=eval("("+t+")");
return o;
}
t=ga("store");
if(t!=null){
o.type="store";
o.data=eval("("+t+")");
t=ga("field");
o.field=t!=null?t:"value";
t=ga("query");
if(!!t){
kw.query=t;
}
t=ga("queryOptions");
if(!!t){
kw.queryOptions=eval("("+t+")");
}
t=ga("start");
if(!!t){
kw.start=Number(t);
}
t=ga("count");
if(!!t){
kw.count=Number(t);
}
t=ga("sort");
if(!!t){
kw.sort=eval("("+t+")");
}
t=ga("valueFn");
if(!!t){
kw.valueFn=_9.lambda(t);
}
return o;
}
return null;
};
return _5("dojox.charting.widget.Chart",_7,{theme:null,margins:null,stroke:undefined,fill:undefined,buildRendering:function(){
this.inherited(arguments);
n=this.domNode;
var _21=_6("> .axis",n).map(_b).filter(_f),_22=_6("> .plot",n).map(_c).filter(_f),_23=_6("> .action",n).map(_d).filter(_f),_24=_6("> .series",n).map(_e).filter(_f);
n.innerHTML="";
var c=this.chart=new _8(n,{margins:this.margins,stroke:this.stroke,fill:this.fill,textDir:this.textDir});
if(this.theme){
c.setTheme(this.theme);
}
_21.forEach(function(_25){
c.addAxis(_25.name,_25.kwArgs);
});
_22.forEach(function(_26){
c.addPlot(_26.name,_26.kwArgs);
});
this.actions=_23.map(function(_27){
return new _27.action(c,_27.plot,_27.kwArgs);
});
var _28=df.foldl(_24,function(_29,_2a){
if(_2a.type=="data"){
c.addSeries(_2a.name,_2a.data,_2a.kwArgs);
_29=true;
}else{
c.addSeries(_2a.name,[0],_2a.kwArgs);
var kw={};
du.updateWithPattern(kw,_2a.kwArgs,{"query":"","queryOptions":null,"start":0,"count":1},true);
if(_2a.kwArgs.sort){
kw.sort=_2.clone(_2a.kwArgs.sort);
}
_2.mixin(kw,{onComplete:function(_2b){
var _2c;
if("valueFn" in _2a.kwArgs){
var fn=_2a.kwArgs.valueFn;
_2c=_3.map(_2b,function(x){
return fn(_2a.data.getValue(x,_2a.field,0));
});
}else{
_2c=_3.map(_2b,function(x){
return _2a.data.getValue(x,_2a.field,0);
});
}
c.addSeries(_2a.name,_2c,_2a.kwArgs).render();
}});
_2a.data.fetch(kw);
}
return _29;
},false);
if(_28){
c.render();
}
},destroy:function(){
this.chart.destroy();
this.inherited(arguments);
},resize:function(box){
this.chart.resize(box);
}});
});
