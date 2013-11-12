//>>built
define("dijit/form/_ListMouseMixin",["dojo/_base/declare","dojo/_base/event","dojo/touch","./_ListBase"],function(_1,_2,_3,_4){
return _1("dijit.form._ListMouseMixin",_4,{postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,_3.press,"_onMouseDown");
this.connect(this.domNode,_3.release,"_onMouseUp");
this.connect(this.domNode,"onmouseover","_onMouseOver");
this.connect(this.domNode,"onmouseout","_onMouseOut");
},_onMouseDown:function(_5){
_2.stop(_5);
if(this._hoveredNode){
this.onUnhover(this._hoveredNode);
this._hoveredNode=null;
}
this._isDragging=true;
this._setSelectedAttr(this._getTarget(_5));
},_onMouseUp:function(_6){
_2.stop(_6);
this._isDragging=false;
var _7=this._getSelectedAttr();
var _8=this._getTarget(_6);
var _9=this._hoveredNode;
if(_7&&_8==_7){
this.onClick(_7);
}else{
if(_9&&_8==_9){
this._setSelectedAttr(_9);
this.onClick(_9);
}
}
},_onMouseOut:function(){
if(this._hoveredNode){
this.onUnhover(this._hoveredNode);
if(this._getSelectedAttr()==this._hoveredNode){
this.onSelect(this._hoveredNode);
}
this._hoveredNode=null;
}
if(this._isDragging){
this._cancelDrag=(new Date()).getTime()+1000;
}
},_onMouseOver:function(_a){
if(this._cancelDrag){
var _b=(new Date()).getTime();
if(_b>this._cancelDrag){
this._isDragging=false;
}
this._cancelDrag=null;
}
var _c=this._getTarget(_a);
if(!_c){
return;
}
if(this._hoveredNode!=_c){
if(this._hoveredNode){
this._onMouseOut({target:this._hoveredNode});
}
if(_c&&_c.parentNode==this.containerNode){
if(this._isDragging){
this._setSelectedAttr(_c);
}else{
this._hoveredNode=_c;
this.onHover(_c);
}
}
}
}});
});
