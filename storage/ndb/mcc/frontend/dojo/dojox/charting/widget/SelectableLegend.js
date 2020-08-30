//>>built
define("dojox/charting/widget/SelectableLegend",["dojo/_base/array","dojo/_base/declare","dojo/query","dojo/_base/connect","dojo/_base/Color","./Legend","dijit/form/CheckBox","../action2d/Highlight","dojox/lang/functional","dojox/gfx/fx","dojo/keys","dojo/dom-construct","dojo/dom-prop","dijit/registry"],function(_1,_2,_3,_4,_5,_6,_7,_8,df,fx,_9,_a,_b,_c){
var _d=_2(null,{constructor:function(_e){
this.legend=_e;
this.index=0;
this.horizontalLength=this._getHrizontalLength();
_1.forEach(_e.legends,function(_f,i){
if(i>0){
_3("input",_f).attr("tabindex",-1);
}
});
this.firstLabel=_3("input",_e.legends[0])[0];
_4.connect(this.firstLabel,"focus",this,function(){
this.legend.active=true;
});
_4.connect(this.legend.domNode,"keydown",this,"_onKeyEvent");
},_getHrizontalLength:function(){
var _10=this.legend.horizontal;
if(typeof _10=="number"){
return Math.min(_10,this.legend.legends.length);
}else{
if(!_10){
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
var _11=_2(_8,{connect:function(){
}});
var _12=_2("dojox.charting.widget.SelectableLegend",_6,{outline:false,transitionFill:null,transitionStroke:null,autoScale:false,postCreate:function(){
this.legends=[];
this.legendAnim={};
this._cbs=[];
this.inherited(arguments);
},refresh:function(){
this.legends=[];
this._clearLabels();
this.inherited(arguments);
this._applyEvents();
new _d(this);
},_clearLabels:function(){
var cbs=this._cbs;
while(cbs.length){
cbs.pop().destroyRecursive();
}
},_addLabel:function(dyn,_13){
this.inherited(arguments);
var _14=_3("td",this.legendBody);
var _15=_14[_14.length-1];
this.legends.push(_15);
var _16=new _7({checked:true});
this._cbs.push(_16);
_a.place(_16.domNode,_15,"first");
var _17=_3("label",_15)[0];
_b.set(_17,"for",_16.id);
},_applyEvents:function(){
if(this.chart.dirty){
return;
}
_1.forEach(this.legends,function(_18,i){
var _19,_1a,_1b;
if(this._isPie()){
_19=this.chart.stack[0];
_1a=_19.name;
_1b=this.chart.series[0].name;
}else{
_19=this.chart.series[i];
_1a=_19.plot;
_1b=_19.name;
}
var _1c=_c.byNode(_3(".dijitCheckBox",_18)[0]);
_1c.set("checked",!this._isHidden(_1a,i));
_4.connect(_1c,"onClick",this,function(e){
this.toogle(_1a,i,!_1c.get("checked"));
e.stopPropagation();
});
var _1d=_3(".dojoxLegendIcon",_18)[0],_1e=this._getFilledShape(this._surfaces[i].children);
_1.forEach(["onmouseenter","onmouseleave"],function(_1f){
_4.connect(_1d,_1f,this,function(e){
this._highlight(e,_1e,i,!_1c.get("checked"),_1b,_1a);
});
},this);
},this);
},_isHidden:function(_20,_21){
if(this._isPie()){
return _1.indexOf(this.chart.getPlot(_20).runFilter,_21)!=-1;
}else{
return this.chart.series[_21].hidden;
}
},toogle:function(_22,_23,_24){
var _25=this.chart.getPlot(_22);
if(this._isPie()){
if(_1.indexOf(_25.runFilter,_23)!=-1){
if(!_24){
_25.runFilter=_1.filter(_25.runFilter,function(_26){
return _26!=_23;
});
}
}else{
if(_24){
_25.runFilter.push(_23);
}
}
}else{
this.chart.series[_23].hidden=_24;
}
this.autoScale?this.chart.dirty=true:_25.dirty=true;
this.chart.render();
},_highlight:function(e,_27,_28,_29,_2a,_2b){
if(!_29){
var _2c=this._getAnim(_2b),_2d=this._isPie(),_2e=_2f(e.type);
var _30={shape:_27,index:_2d?"legend"+_28:"legend",run:{name:_2a},type:_2e};
_2c.process(_30);
_1.forEach(this._getShapes(_28,_2b),function(_31,i){
var o={shape:_31,index:_2d?_28:i,run:{name:_2a},type:_2e};
_2c.duration=100;
_2c.process(o);
});
}
},_getShapes:function(i,_32){
var _33=[];
if(this._isPie()){
var _34=0;
_1.forEach(this.chart.getPlot(_32).runFilter,function(_35){
if(i>_35){
_34++;
}
});
_33.push(this.chart.stack[0].group.children[i-_34]);
}else{
if(this._isCandleStick(_32)){
_1.forEach(this.chart.series[i].group.children,function(_36){
_1.forEach(_36.children,function(_37){
_1.forEach(_37.children,function(_38){
if(_38.shape.type!="line"){
_33.push(_38);
}
});
});
});
}else{
_33=this.chart.series[i].group.children;
}
}
return _33;
},_getAnim:function(_39){
if(!this.legendAnim[_39]){
this.legendAnim[_39]=new _11(this.chart,_39);
}
return this.legendAnim[_39];
},_getTransitionFill:function(_3a){
if(this.chart.stack[this.chart.plots[_3a]].declaredClass.indexOf("dojox.charting.plot2d.Stacked")!=-1){
return this.chart.theme.plotarea.fill;
}
return null;
},_getFilledShape:function(_3b){
var i=0;
while(_3b[i]){
if(_3b[i].getFill()){
return _3b[i];
}
i++;
}
return null;
},_isPie:function(){
return this.chart.stack[0].declaredClass=="dojox.charting.plot2d.Pie";
},_isCandleStick:function(_3c){
return this.chart.stack[this.chart.plots[_3c]].declaredClass=="dojox.charting.plot2d.Candlesticks";
},destroy:function(){
this._clearLabels();
this.inherited(arguments);
}});
function _2f(_3d){
if(_3d=="mouseenter"){
return "onmouseover";
}
if(_3d=="mouseleave"){
return "onmouseout";
}
return "on"+_3d;
};
return _12;
});
