//>>built
define("dojox/dnd/Selector",["dojo","dojox","dojo/dnd/Selector"],function(_1,_2){
return _1.declare("dojox.dnd.Selector",_1.dnd.Selector,{conservative:true,isSelected:function(_3){
var id=_1.isString(_3)?_3:_3.id,_4=this.getItem(id);
return _4&&this.selected[id];
},selectNode:function(_5,_6){
if(!_6){
this.selectNone();
}
var id=_1.isString(_5)?_5:_5.id,_7=this.getItem(id);
if(_7){
this._removeAnchor();
this.anchor=_1.byId(_5);
this._addItemClass(this.anchor,"Anchor");
this.selection[id]=1;
this._addItemClass(this.anchor,"Selected");
}
return this;
},deselectNode:function(_8){
var id=_1.isString(_8)?_8:_8.id,_9=this.getItem(id);
if(_9&&this.selection[id]){
if(this.anchor===_1.byId(_8)){
this._removeAnchor();
}
delete this.selection[id];
this._removeItemClass(this.anchor,"Selected");
}
return this;
},selectByBBox:function(_a,_b,_c,_d,_e){
if(!_e){
this.selectNone();
}
this.forInItems(function(_f,id){
var _10=_1.byId(id);
if(_10&&this._isBoundedByBox(_10,_a,_b,_c,_d)){
this.selectNode(id,true);
}
},this);
return this;
},_isBoundedByBox:function(_11,_12,top,_13,_14){
return this.conservative?this._conservativeBBLogic(_11,_12,top,_13,_14):this._liberalBBLogic(_11,_12,top,_13,_14);
},shift:function(_15,add){
var _16=this.getSelectedNodes();
if(_16&&_16.length){
this.selectNode(this._getNodeId(_16[_16.length-1].id,_15),add);
}
},_getNodeId:function(_17,_18){
var _19=this.getAllNodes(),_1a=_17;
for(var i=0,l=_19.length;i<l;++i){
if(_19[i].id==_17){
var j=Math.min(l-1,Math.max(0,i+(_18?1:-1)));
if(i!=j){
_1a=_19[j].id;
}
break;
}
}
return _1a;
},_conservativeBBLogic:function(_1b,_1c,top,_1d,_1e){
var c=_1.coords(_1b),t;
if(_1c>_1d){
t=_1c;
_1c=_1d;
_1d=t;
}
if(top>_1e){
t=top;
top=_1e;
_1e=t;
}
return c.x>=_1c&&c.x+c.w<=_1d&&c.y>=top&&c.y+c.h<=_1e;
},_liberalBBLogic:function(_1f,_20,top,_21,_22){
var c=_1.position(_1f),_23,_24,tlx,tly,brx,bry,_25=false,_26=false,_27=c.x,_28=c.y,_29=c.x+c.w,_2a=c.y+c.h;
if(_20<_21){
tlx=_20;
tly=top;
}else{
_25=true;
tlx=_21;
tly=_22;
}
if(top<_22){
_26=true;
brx=_21;
bry=_22;
}else{
brx=_20;
bry=top;
tlx=_21;
tly=_22;
}
if(_25&&_26){
brx=_20;
bry=_22;
tlx=_21;
tly=top;
}
_23=(_27>=tlx||_29<=brx)&&(tlx<=_29&&brx>=_27)||(_27<=tlx&&_29>=brx);
_24=(tly<=_2a&&bry>=_28)||(_2a>=bry&&_28<=bry);
return _23&&_24;
}});
});
