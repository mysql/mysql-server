//>>built
define("dojox/grid/_SelectionPreserver",["dojo/_base/declare","dojo/_base/connect","dojo/_base/lang","dojo/_base/array"],function(_1,_2,_3,_4){
return _1("dojox.grid._SelectionPreserver",null,{constructor:function(_5){
this.selection=_5;
var _6=this.grid=_5.grid;
this.reset();
this._connects=[_2.connect(_6,"_setStore",this,"reset"),_2.connect(_6,"_addItem",this,"_reSelectById"),_2.connect(_5,"addToSelection",_3.hitch(this,"_selectById",true)),_2.connect(_5,"deselect",_3.hitch(this,"_selectById",false)),_2.connect(_5,"deselectAll",this,"reset")];
},destroy:function(){
this.reset();
_4.forEach(this._connects,_2.disconnect);
delete this._connects;
},reset:function(){
this._selectedById={};
},_reSelectById:function(_7,_8){
if(_7&&this.grid._hasIdentity){
this.selection.selected[_8]=this._selectedById[this.grid.store.getIdentity(_7)];
}
},_selectById:function(_9,_a){
if(this.selection.mode=="none"||!this.grid._hasIdentity){
return;
}
var _b=_a,g=this.grid;
if(typeof _a=="number"||typeof _a=="string"){
var _c=g._by_idx[_a];
_b=_c&&_c.item;
}
if(_b){
this._selectedById[g.store.getIdentity(_b)]=!!_9;
}
return _b;
}});
});
