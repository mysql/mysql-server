//>>built
define("dojox/charting/plot2d/_PlotEvents",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/connect"],function(_1,_2,_3,_4){
return _3("dojox.charting.plot2d._PlotEvents",null,{constructor:function(){
this._shapeEvents=[];
this._eventSeries={};
},destroy:function(){
this.resetEvents();
this.inherited(arguments);
},plotEvent:function(o){
},raiseEvent:function(o){
this.plotEvent(o);
var t=_1.delegate(o);
t.originalEvent=o.type;
t.originalPlot=o.plot;
t.type="onindirect";
_2.forEach(this.chart.stack,function(_5){
if(_5!==this&&_5.plotEvent){
t.plot=_5;
_5.plotEvent(t);
}
},this);
},connect:function(_6,_7){
this.dirty=true;
return _4.connect(this,"plotEvent",_6,_7);
},events:function(){
return !!this.plotEvent.after;
},resetEvents:function(){
if(this._shapeEvents.length){
_2.forEach(this._shapeEvents,function(_8){
_8.shape.disconnect(_8.handle);
});
this._shapeEvents=[];
}
this.raiseEvent({type:"onplotreset",plot:this});
},_connectSingleEvent:function(o,_9){
this._shapeEvents.push({shape:o.eventMask,handle:o.eventMask.connect(_9,this,function(e){
o.type=_9;
o.event=e;
this.raiseEvent(o);
o.event=null;
})});
},_connectEvents:function(o){
if(o){
o.chart=this.chart;
o.plot=this;
o.hAxis=this.hAxis||null;
o.vAxis=this.vAxis||null;
o.eventMask=o.eventMask||o.shape;
this._connectSingleEvent(o,"onmouseover");
this._connectSingleEvent(o,"onmouseout");
this._connectSingleEvent(o,"onclick");
}
},_reconnectEvents:function(_a){
var a=this._eventSeries[_a];
if(a){
_2.forEach(a,this._connectEvents,this);
}
},fireEvent:function(_b,_c,_d,_e){
var s=this._eventSeries[_b];
if(s&&s.length&&_d<s.length){
var o=s[_d];
o.type=_c;
o.event=_e||null;
this.raiseEvent(o);
o.event=null;
}
}});
});
