//>>built
define("dojox/grid/enhanced/plugins/_SelectionPreserver",["dojo/_base/declare","dojo/_base/lang","dojo/_base/connect","../../_SelectionPreserver"],function(_1,_2,_3,_4){
return _1("dojox.grid.enhanced.plugins._SelectionPreserver",_4,{constructor:function(_5){
var _6=this.grid;
_6.onSelectedById=this.onSelectedById;
this._oldClearData=_6._clearData;
var _7=this;
_6._clearData=function(){
_7._updateMapping(!_6._noInternalMapping);
_7._trustSelection=[];
_7._oldClearData.apply(_6,arguments);
};
this._connects.push(_3.connect(_5,"selectRange",_2.hitch(this,"_updateMapping",true,true,false)),_3.connect(_5,"deselectRange",_2.hitch(this,"_updateMapping",true,false,false)),_3.connect(_5,"deselectAll",_2.hitch(this,"_updateMapping",true,false,true)));
},destroy:function(){
this.inherited(arguments);
this.grid._clearData=this._oldClearData;
},reset:function(){
this.inherited(arguments);
this._idMap=[];
this._trustSelection=[];
this._defaultSelected=false;
},_reSelectById:function(_8,_9){
var s=this.selection,g=this.grid;
if(_8&&g._hasIdentity){
var id=g.store.getIdentity(_8);
if(this._selectedById[id]===undefined){
if(!this._trustSelection[_9]){
s.selected[_9]=this._defaultSelected;
}
}else{
s.selected[_9]=this._selectedById[id];
}
this._idMap.push(id);
g.onSelectedById(id,_9,s.selected[_9]);
}
},_selectById:function(_a,_b){
if(!this.inherited(arguments)){
this._trustSelection[_b]=true;
}
},onSelectedById:function(id,_c,_d){
},_updateMapping:function(_e,_f,_10,_11,to){
var s=this.selection,g=this.grid,_12=0,_13=0,i,id;
for(i=g.rowCount-1;i>=0;--i){
if(!g._by_idx[i]){
++_13;
_12+=s.selected[i]?1:-1;
}else{
id=g._by_idx[i].idty;
if(id&&(_e||this._selectedById[id]===undefined)){
this._selectedById[id]=!!s.selected[i];
}
}
}
if(_13){
this._defaultSelected=_12>0;
}
if(!_10&&_11!==undefined&&to!==undefined){
_10=!g.usingPagination&&Math.abs(to-_11+1)===g.rowCount;
}
if(_10&&(!g.usingPagination||g.selectionMode==="single")){
for(i=this._idMap.length-1;i>=0;--i){
this._selectedById[this._idMap[i]]=_f;
}
}
}});
});
