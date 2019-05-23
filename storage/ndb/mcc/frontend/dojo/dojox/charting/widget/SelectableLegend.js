//>>built
define("dojox/charting/widget/SelectableLegend",["dojo/_base/array","dojo/_base/declare","dojo/query","dojo/_base/connect","dojo/_base/Color","./Legend","dijit/form/CheckBox","../action2d/Highlight","dojox/lang/functional","dojox/gfx/fx","dojo/keys","dojo/dom-construct","dojo/dom-prop"],function(_1,_2,_3,_4,_5,_6,_7,_8,df,fx,_9,_a,_b){
var _c=_2(null,{constructor:function(_d){
this.legend=_d;
this.index=0;
this.horizontalLength=this._getHrizontalLength();
_1.forEach(_d.legends,function(_e,i){
if(i>0){
_3("input",_e).attr("tabindex",-1);
}
});
this.firstLabel=_3("input",_d.legends[0])[0];
_4.connect(this.firstLabel,"focus",this,function(){
this.legend.active=true;
});
_4.connect(this.legend.domNode,"keydown",this,"_onKeyEvent");
},_getHrizontalLength:function(){
var _f=this.legend.horizontal;
if(typeof _f=="number"){
return Math.min(_f,this.legend.legends.length);
}else{
if(!_f){
return 1;
}else{
return this.legend.legends.length;
}
}
},_onKeyEvent:function(e){
if(!this.legend.active){
return;
}
if(e.keyCode==_9.TAB){
this.legend.active=false;
return;
}
var max=this.legend.legends.length;
switch(e.keyCode){
case _9.LEFT_ARROW:
this.index--;
if(this.index<0){
this.index+=max;
}
break;
case _9.RIGHT_ARROW:
this.index++;
if(this.index>=max){
this.index-=max;
}
break;
case _9.UP_ARROW:
if(this.index-this.horizontalLength>=0){
this.index-=this.horizontalLength;
}
break;
case _9.DOWN_ARROW:
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
_3("input",this.legend.legends[this.index])[0].focus();
}});
var _10=_2("dojox.charting.widget.SelectableLegend",_6,{outline:false,transitionFill:null,transitionStroke:null,postCreate:function(){
this.legends=[];
this.legendAnim={};
this._cbs=[];
this.inherited(arguments);
},refresh:function(){
this.legends=[];
this._clearLabels();
this.inherited(arguments);
this._applyEvents();
new _c(this);
},_clearLabels:function(){
var cbs=this._cbs;
while(cbs.length){
cbs.pop().destroyRecursive();
}
},_addLabel:function(dyn,_11){
this.inherited(arguments);
var _12=_3("td",this.legendBody);
var _13=_12[_12.length-1];
this.legends.push(_13);
var _14=new _7({checked:true});
this._cbs.push(_14);
_a.place(_14.domNode,_13,"first");
var _15=_3("label",_13)[0];
_b.set(_15,"for",_14.id);
},_applyEvents:function(){
if(this.chart.dirty){
return;
}
_1.forEach(this.legends,function(_16,i){
var _17,_18=[],_19,_1a;
if(this._isPie()){
_17=this.chart.stack[0];
_18.push(_17.group.children[i]);
_19=_17.name;
_1a=this.chart.series[0].name;
}else{
_17=this.chart.series[i];
_18=_17.group.children;
_19=_17.plot;
_1a=_17.name;
}
var _1b={fills:df.map(_18,"x.getFill()"),strokes:df.map(_18,"x.getStroke()")};
var _1c=_3(".dijitCheckBox",_16)[0];
_4.connect(_1c,"onclick",this,function(e){
this._toggle(_18,i,_16.vanished,_1b,_1a,_19);
_16.vanished=!_16.vanished;
e.stopPropagation();
});
var _1d=_3(".dojoxLegendIcon",_16)[0],_1e=this._getFilledShape(this._surfaces[i].children);
_1.forEach(["onmouseenter","onmouseleave"],function(_1f){
_4.connect(_1d,_1f,this,function(e){
this._highlight(e,_1e,_18,i,_16.vanished,_1b,_1a,_19);
});
},this);
},this);
},_toggle:function(_20,_21,_22,dyn,_23,_24){
_1.forEach(_20,function(_25,i){
var _26=dyn.fills[i],_27=this._getTransitionFill(_24),_28=dyn.strokes[i],_29=this.transitionStroke;
if(_26){
if(_27&&(typeof _26=="string"||_26 instanceof _5)){
fx.animateFill({shape:_25,color:{start:_22?_27:_26,end:_22?_26:_27}}).play();
}else{
_25.setFill(_22?_26:_27);
}
}
if(_28&&!this.outline){
_25.setStroke(_22?_28:_29);
}
},this);
},_highlight:function(e,_2a,_2b,_2c,_2d,dyn,_2e,_2f){
if(!_2d){
var _30=this._getAnim(_2f),_31=this._isPie(),_32=_33(e.type);
var _34={shape:_2a,index:_31?"legend"+_2c:"legend",run:{name:_2e},type:_32};
_30.process(_34);
_1.forEach(_2b,function(_35,i){
_35.setFill(dyn.fills[i]);
var o={shape:_35,index:_31?_2c:i,run:{name:_2e},type:_32};
_30.duration=100;
_30.process(o);
});
}
},_getAnim:function(_36){
if(!this.legendAnim[_36]){
this.legendAnim[_36]=new _8(this.chart,_36);
this.chart.getPlot(_36).dirty=false;
}
return this.legendAnim[_36];
},_getTransitionFill:function(_37){
if(this.chart.stack[this.chart.plots[_37]].declaredClass.indexOf("dojox.charting.plot2d.Stacked")!=-1){
return this.chart.theme.plotarea.fill;
}
return null;
},_getFilledShape:function(_38){
var i=0;
while(_38[i]){
if(_38[i].getFill()){
return _38[i];
}
i++;
}
return null;
},_isPie:function(){
return this.chart.stack[0].declaredClass=="dojox.charting.plot2d.Pie";
},destroy:function(){
this._clearLabels();
this.inherited(arguments);
}});
function _33(_39){
if(_39=="mouseenter"){
return "onmouseover";
}
if(_39=="mouseleave"){
return "onmouseout";
}
return "on"+_39;
};
return _10;
});
