//>>built
define("dojox/grid/enhanced/plugins/Dialog",["dojo/_base/declare","dojo/_base/html","dojo/window","dijit/Dialog"],function(_1,_2,_3,_4){
return _1("dojox.grid.enhanced.plugins.Dialog",_4,{refNode:null,_position:function(){
if(this.refNode&&!this._relativePosition){
var _5=_2.position(_2.byId(this.refNode)),_6=_2.position(this.domNode),_7=_3.getBox();
if(_6.w&&_6.h){
if(_5.x<0){
_5.x=0;
}
if(_5.x+_5.w>_7.w){
_5.w=_7.w-_5.x;
}
if(_5.y<0){
_5.y=0;
}
if(_5.y+_5.h>_7.h){
_5.h=_7.h-_5.y;
}
_5.x=_5.x+_5.w/2-_6.w/2;
_5.y=_5.y+_5.h/2-_6.h/2;
if(_5.x>=0&&_5.x+_6.w<=_7.w&&_5.y>=0&&_5.y+_6.h<=_7.h){
this._relativePosition=_5;
}
}
}
this.inherited(arguments);
}});
});
