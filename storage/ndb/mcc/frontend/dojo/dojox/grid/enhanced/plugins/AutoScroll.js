//>>built
define("dojox/grid/enhanced/plugins/AutoScroll",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/html","dojo/_base/window","../_Plugin","../../_RowSelector","../../EnhancedGrid"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_1("dojox.grid.enhanced.plugins.AutoScroll",_6,{name:"autoScroll",autoScrollInterval:1000,autoScrollMargin:30,constructor:function(_a,_b){
this.grid=_a;
this.readyForAutoScroll=false;
this._scrolling=false;
_b=_3.isObject(_b)?_b:{};
if("interval" in _b){
this.autoScrollInterval=_b.interval;
}
if("margin" in _b){
this.autoScrollMargin=_b.margin;
}
this._initEvents();
this._mixinGrid();
},_initEvents:function(){
var g=this.grid;
this.connect(g,"onCellMouseDown",function(){
this.readyForAutoScroll=true;
});
this.connect(g,"onHeaderCellMouseDown",function(){
this.readyForAutoScroll=true;
});
this.connect(g,"onRowSelectorMouseDown",function(){
this.readyForAutoScroll=true;
});
this.connect(_5.doc,"onmouseup",function(_c){
this._manageAutoScroll(true);
this.readyForAutoScroll=false;
});
this.connect(_5.doc,"onmousemove",function(_d){
if(this.readyForAutoScroll){
this._event=_d;
var _e=_4.position(g.domNode),hh=g._getHeaderHeight(),_f=this.autoScrollMargin,ey=_d.clientY,ex=_d.clientX,gy=_e.y,gx=_e.x,gh=_e.h,gw=_e.w;
if(ex>=gx&&ex<=gx+gw){
if(ey>=gy+hh&&ey<gy+hh+_f){
this._manageAutoScroll(false,true,false);
return;
}else{
if(ey>gy+gh-_f&&ey<=gy+gh){
this._manageAutoScroll(false,true,true);
return;
}else{
if(ey>=gy&&ey<=gy+gh){
var _10=_2.some(g.views.views,function(_11,i){
if(_11 instanceof _7){
return false;
}
var _12=_4.position(_11.domNode);
if(ex<_12.x+_f&&ex>=_12.x){
this._manageAutoScroll(false,false,false,_11);
return true;
}else{
if(ex>_12.x+_12.w-_f&&ex<_12.x+_12.w){
this._manageAutoScroll(false,false,true,_11);
return true;
}
}
return false;
},this);
if(_10){
return;
}
}
}
}
}
this._manageAutoScroll(true);
}
});
},_mixinGrid:function(){
var g=this.grid;
g.onStartAutoScroll=function(){
};
g.onEndAutoScroll=function(){
};
},_fireEvent:function(_13,_14){
var g=this.grid;
switch(_13){
case "start":
g.onStartAutoScroll.apply(g,_14);
break;
case "end":
g.onEndAutoScroll.apply(g,_14);
break;
}
},_manageAutoScroll:function(_15,_16,_17,_18){
if(_15){
this._scrolling=false;
clearInterval(this._handler);
}else{
if(!this._scrolling){
this._scrolling=true;
this._fireEvent("start",[_16,_17,_18]);
this._autoScroll(_16,_17,_18);
this._handler=setInterval(_3.hitch(this,"_autoScroll",_16,_17,_18),this.autoScrollInterval);
}
}
},_autoScroll:function(_19,_1a,_1b){
var g=this.grid,_1c=null;
if(_19){
var _1d=g.scroller.firstVisibleRow+(_1a?1:-1);
if(_1d>=0&&_1d<g.rowCount){
g.scrollToRow(_1d);
_1c=_1d;
}
}else{
_1c=this._scrollColumn(_1a,_1b);
}
if(_1c!==null){
this._fireEvent("end",[_19,_1a,_1b,_1c,this._event]);
}
},_scrollColumn:function(_1e,_1f){
var _20=_1f.scrollboxNode,_21=null;
if(_20.clientWidth<_20.scrollWidth){
var _22=_2.filter(this.grid.layout.cells,function(_23){
return !_23.hidden;
});
var _24=_4.position(_1f.domNode);
var _25,_26,_27,i;
if(_1e){
_25=_20.clientWidth;
for(i=0;i<_22.length;++i){
_27=_4.position(_22[i].getHeaderNode());
_26=_27.x-_24.x+_27.w;
if(_26>_25){
_21=_22[i].index;
_20.scrollLeft+=_26-_25+10;
break;
}
}
}else{
_25=0;
for(i=_22.length-1;i>=0;--i){
_27=_4.position(_22[i].getHeaderNode());
_26=_27.x-_24.x;
if(_26<_25){
_21=_22[i].index;
_20.scrollLeft+=_26-_25-10;
break;
}
}
}
}
return _21;
}});
_8.registerPlugin(_9);
return _9;
});
