//>>built
define("dojox/charting/widget/Chart",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/dom-attr","dojo/_base/declare","dojo/query","dijit/_WidgetBase","../Chart","dojo/has","dojo/has!dojo-bidi?../bidi/widget/Chart","dojox/lang/utils","dojox/lang/functional","dojox/lang/functional/lambda"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,du,df,_b){
var _c,_d,_e,_f,_10,_11=function(o){
return o;
},dc=_2.getObject("dojox.charting");
_c=function(_12,_13,kw){
var dp=eval("("+_13+".prototype.defaultParams)");
var x,_14;
for(x in dp){
if(x in kw){
continue;
}
_14=_12.getAttribute(x);
kw[x]=du.coerceType(dp[x],_14==null||typeof _14=="undefined"?dp[x]:_14);
}
var op=eval("("+_13+".prototype.optionalParams)");
for(x in op){
if(x in kw){
continue;
}
_14=_12.getAttribute(x);
if(_14!=null){
kw[x]=du.coerceType(op[x],_14);
}
}
};
_d=function(_15){
var _16=_15.getAttribute("name"),_17=_15.getAttribute("type");
if(!_16){
return null;
}
var o={name:_16,kwArgs:{}},kw=o.kwArgs;
if(_17){
if(dc.axis2d[_17]){
_17=_1._scopeName+"x.charting.axis2d."+_17;
}
var _18=eval("("+_17+")");
if(_18){
kw.type=_18;
}
}else{
_17=_1._scopeName+"x.charting.axis2d.Default";
}
_c(_15,_17,kw);
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
_e=function(_19){
var _1a=_19.getAttribute("name"),_1b=_19.getAttribute("type");
if(!_1a){
return null;
}
var o={name:_1a,kwArgs:{}},kw=o.kwArgs;
if(_1b){
if(dc.plot2d&&dc.plot2d[_1b]){
_1b=_1._scopeName+"x.charting.plot2d."+_1b;
}
var _1c=eval("("+_1b+")");
if(_1c){
kw.type=_1c;
}
}else{
_1b=_1._scopeName+"x.charting.plot2d.Default";
}
_c(_19,_1b,kw);
var dp=eval("("+_1b+".prototype.baseParams)");
var x,_1d;
for(x in dp){
if(x in kw){
continue;
}
_1d=_19.getAttribute(x);
kw[x]=du.coerceType(dp[x],_1d==null||typeof _1d=="undefined"?dp[x]:_1d);
}
return o;
};
_f=function(_1e){
var _1f=_1e.getAttribute("plot"),_20=_1e.getAttribute("type");
if(!_1f){
_1f="default";
}
var o={plot:_1f,kwArgs:{}},kw=o.kwArgs;
if(_20){
if(dc.action2d[_20]){
_20=_1._scopeName+"x.charting.action2d."+_20;
}
var _21=eval("("+_20+")");
if(!_21){
return null;
}
o.action=_21;
}else{
return null;
}
_c(_1e,_20,kw);
return o;
};
_10=function(_22){
var ga=_2.partial(_4.get,_22);
var _23=ga("name");
if(!_23){
return null;
}
var o={name:_23,kwArgs:{}},kw=o.kwArgs,t;
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
kw.valueFn=_b.lambda(t);
}
return o;
}
return null;
};
var _24=_5(_9("dojo-bidi")?"dojox.charting.widget.NonBidiChart":"dojox.charting.widget.Chart",_7,{theme:null,margins:null,stroke:undefined,fill:undefined,buildRendering:function(){
this.inherited(arguments);
var n=this.domNode;
var _25=_6("> .axis",n).map(_d).filter(_11),_26=_6("> .plot",n).map(_e).filter(_11),_27=_6("> .action",n).map(_f).filter(_11),_28=_6("> .series",n).map(_10).filter(_11);
n.innerHTML="";
var c=this.chart=new _8(n,{margins:this.margins,stroke:this.stroke,fill:this.fill,textDir:this.textDir});
if(this.theme){
c.setTheme(this.theme);
}
_25.forEach(function(_29){
c.addAxis(_29.name,_29.kwArgs);
});
_26.forEach(function(_2a){
c.addPlot(_2a.name,_2a.kwArgs);
});
this.actions=_27.map(function(_2b){
return new _2b.action(c,_2b.plot,_2b.kwArgs);
});
var _2c=df.foldl(_28,function(_2d,_2e){
if(_2e.type=="data"){
c.addSeries(_2e.name,_2e.data,_2e.kwArgs);
_2d=true;
}else{
c.addSeries(_2e.name,[0],_2e.kwArgs);
var kw={};
du.updateWithPattern(kw,_2e.kwArgs,{"query":"","queryOptions":null,"start":0,"count":1},true);
if(_2e.kwArgs.sort){
kw.sort=_2.clone(_2e.kwArgs.sort);
}
_2.mixin(kw,{onComplete:function(_2f){
var _30;
if("valueFn" in _2e.kwArgs){
var fn=_2e.kwArgs.valueFn;
_30=_3.map(_2f,function(x){
return fn(_2e.data.getValue(x,_2e.field,0));
});
}else{
_30=_3.map(_2f,function(x){
return _2e.data.getValue(x,_2e.field,0);
});
}
c.addSeries(_2e.name,_30,_2e.kwArgs).render();
}});
_2e.data.fetch(kw);
}
return _2d;
},false);
if(_2c){
c.render();
}
},destroy:function(){
this.chart.destroy();
this.inherited(arguments);
},resize:function(box){
this.chart.resize.apply(this.chart,arguments);
}});
return _9("dojo-bidi")?_5("dojox.charting.widget.Chart",[_24,_a]):_24;
});
