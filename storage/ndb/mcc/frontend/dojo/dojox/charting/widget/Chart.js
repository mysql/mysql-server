//>>built
define("dojox/charting/widget/Chart",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/html","dojo/_base/declare","dojo/query","dijit/_Widget","../Chart","dojox/lang/utils","dojox/lang/functional","dojox/lang/functional/lambda","dijit/_base/manager"],function(_1,_2,_3,_4,_5,_6,_7,_8,du,df,_9){
var _a,_b,_c,_d,_e,_f=function(o){
return o;
},dc=_2.getObject("dojox.charting");
var _10=_5("dojox.charting.widget.Chart",_7,{theme:null,margins:null,stroke:undefined,fill:undefined,buildRendering:function(){
this.inherited(arguments);
n=this.domNode;
var _11=_6("> .axis",n).map(_b).filter(_f),_12=_6("> .plot",n).map(_c).filter(_f),_13=_6("> .action",n).map(_d).filter(_f),_14=_6("> .series",n).map(_e).filter(_f);
n.innerHTML="";
var c=this.chart=new _8(n,{margins:this.margins,stroke:this.stroke,fill:this.fill,textDir:this.textDir});
if(this.theme){
c.setTheme(this.theme);
}
_11.forEach(function(_15){
c.addAxis(_15.name,_15.kwArgs);
});
_12.forEach(function(_16){
c.addPlot(_16.name,_16.kwArgs);
});
this.actions=_13.map(function(_17){
return new _17.action(c,_17.plot,_17.kwArgs);
});
var _18=df.foldl(_14,function(_19,_1a){
if(_1a.type=="data"){
c.addSeries(_1a.name,_1a.data,_1a.kwArgs);
_19=true;
}else{
c.addSeries(_1a.name,[0],_1a.kwArgs);
var kw={};
du.updateWithPattern(kw,_1a.kwArgs,{"query":"","queryOptions":null,"start":0,"count":1},true);
if(_1a.kwArgs.sort){
kw.sort=_2.clone(_1a.kwArgs.sort);
}
_2.mixin(kw,{onComplete:function(_1b){
var _1c;
if("valueFn" in _1a.kwArgs){
var fn=_1a.kwArgs.valueFn;
_1c=_3.map(_1b,function(x){
return fn(_1a.data.getValue(x,_1a.field,0));
});
}else{
_1c=_3.map(_1b,function(x){
return _1a.data.getValue(x,_1a.field,0);
});
}
c.addSeries(_1a.name,_1c,_1a.kwArgs).render();
}});
_1a.data.fetch(kw);
}
return _19;
},false);
if(_18){
c.render();
}
},destroy:function(){
this.chart.destroy();
this.inherited(arguments);
},resize:function(box){
this.chart.resize(box);
}});
_a=function(_1d,_1e,kw){
var dp=eval("("+_1e+".prototype.defaultParams)");
var x,_1f;
for(x in dp){
if(x in kw){
continue;
}
_1f=_1d.getAttribute(x);
kw[x]=du.coerceType(dp[x],_1f==null||typeof _1f=="undefined"?dp[x]:_1f);
}
var op=eval("("+_1e+".prototype.optionalParams)");
for(x in op){
if(x in kw){
continue;
}
_1f=_1d.getAttribute(x);
if(_1f!=null){
kw[x]=du.coerceType(op[x],_1f);
}
}
};
_b=function(_20){
var _21=_20.getAttribute("name"),_22=_20.getAttribute("type");
if(!_21){
return null;
}
var o={name:_21,kwArgs:{}},kw=o.kwArgs;
if(_22){
if(dc.axis2d[_22]){
_22=dojo._scopeName+"x.charting.axis2d."+_22;
}
var _23=eval("("+_22+")");
if(_23){
kw.type=_23;
}
}else{
_22=dojo._scopeName+"x.charting.axis2d.Default";
}
_a(_20,_22,kw);
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
_c=function(_24){
var _25=_24.getAttribute("name"),_26=_24.getAttribute("type");
if(!_25){
return null;
}
var o={name:_25,kwArgs:{}},kw=o.kwArgs;
if(_26){
if(dc.plot2d&&dc.plot2d[_26]){
_26=dojo._scopeName+"x.charting.plot2d."+_26;
}
var _27=eval("("+_26+")");
if(_27){
kw.type=_27;
}
}else{
_26=dojo._scopeName+"x.charting.plot2d.Default";
}
_a(_24,_26,kw);
return o;
};
_d=function(_28){
var _29=_28.getAttribute("plot"),_2a=_28.getAttribute("type");
if(!_29){
_29="default";
}
var o={plot:_29,kwArgs:{}},kw=o.kwArgs;
if(_2a){
if(dc.action2d[_2a]){
_2a=dojo._scopeName+"x.charting.action2d."+_2a;
}
var _2b=eval("("+_2a+")");
if(!_2b){
return null;
}
o.action=_2b;
}else{
return null;
}
_a(_28,_2a,kw);
return o;
};
_e=function(_2c){
var ga=_2.partial(_4.attr,_2c);
var _2d=ga("name");
if(!_2d){
return null;
}
var o={name:_2d,kwArgs:{}},kw=o.kwArgs,t;
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
return _10;
});
