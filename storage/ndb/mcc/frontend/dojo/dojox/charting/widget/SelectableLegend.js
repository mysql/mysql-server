//>>built
define("dojox/charting/widget/SelectableLegend",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/query","dojo/_base/html","dojo/_base/connect","dojo/_base/Color","./Legend","dijit/form/CheckBox","../action2d/Highlight","dojox/lang/functional","dojox/gfx/fx","dojo/keys","dojo/_base/event","dojo/dom-construct","dojo/dom-prop"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,df,fx,_b,_c,_d,_e){
var _f=_3(null,{constructor:function(_10){
this.legend=_10;
this.index=0;
this.horizontalLength=this._getHrizontalLength();
_2.forEach(_10.legends,function(_11,i){
if(i>0){
_4("input",_11).attr("tabindex",-1);
}
});
this.firstLabel=_4("input",_10.legends[0])[0];
_6.connect(this.firstLabel,"focus",this,function(){
this.legend.active=true;
});
_6.connect(this.legend.domNode,"keydown",this,"_onKeyEvent");
},_getHrizontalLength:function(){
var _12=this.legend.horizontal;
if(typeof _12=="number"){
return Math.min(_12,this.legend.legends.length);
}else{
if(!_12){
return 1;
}else{
return this.legend.legends.length;
}
}
},_onKeyEvent:function(e){
if(!this.legend.active){
return;
}
if(e.keyCode==_b.TAB){
this.legend.active=false;
return;
}
var max=this.legend.legends.length;
switch(e.keyCode){
case _b.LEFT_ARROW:
this.index--;
if(this.index<0){
this.index+=max;
}
break;
case _b.RIGHT_ARROW:
this.index++;
if(this.index>=max){
this.index-=max;
}
break;
case _b.UP_ARROW:
if(this.index-this.horizontalLength>=0){
this.index-=this.horizontalLength;
}
break;
case _b.DOWN_ARROW:
if(this.index+this.horizontalLength<max){
this.index+=this.horizontalLength;
}
break;
default:
return;
}
this._moveToFocus();
Event.stop(e);
},_moveToFocus:function(){
_4("input",this.legend.legends[this.index])[0].focus();
}});
_3("dojox.charting.widget.SelectableLegend",_8,{outline:false,transitionFill:null,transitionStroke:null,postCreate:function(){
this.legends=[];
this.legendAnim={};
this.inherited(arguments);
},refresh:function(){
this.legends=[];
this.inherited(arguments);
this._applyEvents();
new _f(this);
},_addLabel:function(dyn,_13){
this.inherited(arguments);
var _14=_4("td",this.legendBody);
var _15=_14[_14.length-1];
this.legends.push(_15);
var _16=new _9({checked:true});
_d.place(_16.domNode,_15,"first");
var _13=_4("label",_15)[0];
_e.set(_13,"for",_16.id);
},_applyEvents:function(){
if(this.chart.dirty){
return;
}
_2.forEach(this.legends,function(_17,i){
var _18,_19=[],_1a,_1b;
if(this._isPie()){
_18=this.chart.stack[0];
_19.push(_18.group.children[i]);
_1a=_18.name;
_1b=this.chart.series[0].name;
}else{
_18=this.chart.series[i];
_19=_18.group.children;
_1a=_18.plot;
_1b=_18.name;
}
var _1c={fills:df.map(_19,"x.getFill()"),strokes:df.map(_19,"x.getStroke()")};
var _1d=_4(".dijitCheckBox",_17)[0];
_6.connect(_1d,"onclick",this,function(e){
this._toggle(_19,i,_17.vanished,_1c,_1b,_1a);
_17.vanished=!_17.vanished;
e.stopPropagation();
});
var _1e=_4(".dojoxLegendIcon",_17)[0],_1f=this._getFilledShape(this._surfaces[i].children);
_2.forEach(["onmouseenter","onmouseleave"],function(_20){
_6.connect(_1e,_20,this,function(e){
this._highlight(e,_1f,_19,i,_17.vanished,_1c,_1b,_1a);
});
},this);
},this);
},_toggle:function(_21,_22,_23,dyn,_24,_25){
_2.forEach(_21,function(_26,i){
var _27=dyn.fills[i],_28=this._getTransitionFill(_25),_29=dyn.strokes[i],_2a=this.transitionStroke;
if(_27){
if(_28&&(typeof _27=="string"||_27 instanceof _7)){
fx.animateFill({shape:_26,color:{start:_23?_28:_27,end:_23?_27:_28}}).play();
}else{
_26.setFill(_23?_27:_28);
}
}
if(_29&&!this.outline){
_26.setStroke(_23?_29:_2a);
}
},this);
},_highlight:function(e,_2b,_2c,_2d,_2e,dyn,_2f,_30){
if(!_2e){
var _31=this._getAnim(_30),_32=this._isPie(),_33=_34(e.type);
var _35={shape:_2b,index:_32?"legend"+_2d:"legend",run:{name:_2f},type:_33};
_31.process(_35);
_2.forEach(_2c,function(_36,i){
_36.setFill(dyn.fills[i]);
var o={shape:_36,index:_32?_2d:i,run:{name:_2f},type:_33};
_31.duration=100;
_31.process(o);
});
}
},_getAnim:function(_37){
if(!this.legendAnim[_37]){
this.legendAnim[_37]=new _a(this.chart,_37);
}
return this.legendAnim[_37];
},_getTransitionFill:function(_38){
if(this.chart.stack[this.chart.plots[_38]].declaredClass.indexOf("dojox.charting.plot2d.Stacked")!=-1){
return this.chart.theme.plotarea.fill;
}
return null;
},_getFilledShape:function(_39){
var i=0;
while(_39[i]){
if(_39[i].getFill()){
return _39[i];
}
i++;
}
},_isPie:function(){
return this.chart.stack[0].declaredClass=="dojox.charting.plot2d.Pie";
}});
function _34(_3a){
if(_3a=="mouseenter"){
return "onmouseover";
}
if(_3a=="mouseleave"){
return "onmouseout";
}
return "on"+_3a;
};
return dojox.charting.widget.SelectableLegend;
});
