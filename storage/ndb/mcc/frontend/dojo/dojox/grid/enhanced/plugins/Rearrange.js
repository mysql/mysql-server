//>>built
define("dojox/grid/enhanced/plugins/Rearrange",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","../../EnhancedGrid","../_Plugin","./_RowMapLayer"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_3("dojox.grid.enhanced.plugins.Rearrange",_7,{name:"rearrange",constructor:function(_a,_b){
this.grid=_a;
this.setArgs(_b);
var _c=new _8(_a);
dojox.grid.enhanced.plugins.wrap(_a,"_storeLayerFetch",_c);
},setArgs:function(_d){
this.args=_2.mixin(this.args||{},_d||{});
this.args.setIdentifierForNewItem=this.args.setIdentifierForNewItem||function(v){
return v;
};
},destroy:function(){
this.inherited(arguments);
this.grid.unwrap("rowmap");
},onSetStore:function(_e){
this.grid.layer("rowmap").clearMapping();
},_hasIdentity:function(_f){
var g=this.grid,s=g.store,_10=g.layout.cells;
if(s.getFeatures()["dojo.data.api.Identity"]){
if(_4.some(_f,function(_11){
return s.getIdentityAttributes(g._by_idx[_11.r].item)==_10[_11.c].field;
})){
return true;
}
}
return false;
},moveColumns:function(_12,_13){
var g=this.grid,_14=g.layout,_15=_14.cells,_16,i,_17=0,_18=true,tmp={},_19={};
_12.sort(function(a,b){
return a-b;
});
for(i=0;i<_12.length;++i){
tmp[_12[i]]=i;
if(_12[i]<_13){
++_17;
}
}
var _1a=0,_1b=0;
var _1c=Math.max(_12[_12.length-1],_13);
if(_1c==_15.length){
--_1c;
}
var _1d=Math.min(_12[0],_13);
for(i=_1d;i<=_1c;++i){
var j=tmp[i];
if(j>=0){
_19[i]=_13-_17+j;
}else{
if(i<_13){
_19[i]=_1d+_1a;
++_1a;
}else{
if(i>=_13){
_19[i]=_13+_12.length-_17+_1b;
++_1b;
}
}
}
}
_17=0;
if(_13==_15.length){
--_13;
_18=false;
}
g._notRefreshSelection=true;
for(i=0;i<_12.length;++i){
_16=_12[i];
if(_16<_13){
_16-=_17;
}
++_17;
if(_16!=_13){
_14.moveColumn(_15[_16].view.idx,_15[_13].view.idx,_16,_13,_18);
_15=_14.cells;
}
if(_13<=_16){
++_13;
}
}
delete g._notRefreshSelection;
_5.publish("dojox/grid/rearrange/move/"+g.id,["col",_19,_12]);
},moveRows:function(_1e,_1f){
var g=this.grid,_20={},_21=[],_22=[],len=_1e.length,i,r,k,arr,_23,_24;
for(i=0;i<len;++i){
r=_1e[i];
if(r>=_1f){
break;
}
_21.push(r);
}
_22=_1e.slice(i);
arr=_21;
len=arr.length;
if(len){
_23={};
_4.forEach(arr,function(r){
_23[r]=true;
});
_20[arr[0]]=_1f-len;
for(k=0,i=arr[k]+1,_24=i-1;i<_1f;++i){
if(!_23[i]){
_20[i]=_24;
++_24;
}else{
++k;
_20[i]=_1f-len+k;
}
}
}
arr=_22;
len=arr.length;
if(len){
_23={};
_4.forEach(arr,function(r){
_23[r]=true;
});
_20[arr[len-1]]=_1f+len-1;
for(k=len-1,i=arr[k]-1,_24=i+1;i>=_1f;--i){
if(!_23[i]){
_20[i]=_24;
--_24;
}else{
--k;
_20[i]=_1f+k;
}
}
}
var _25=_2.clone(_20);
g.layer("rowmap").setMapping(_20);
g.forEachLayer(function(_26){
if(_26.name()!="rowmap"){
_26.invalidate();
return true;
}else{
return false;
}
},false);
g.selection.selected=[];
g._noInternalMapping=true;
g._refresh();
setTimeout(function(){
_5.publish("dojox/grid/rearrange/move/"+g.id,["row",_25,_1e]);
g._noInternalMapping=false;
},0);
},moveCells:function(_27,_28){
var g=this.grid,s=g.store;
if(s.getFeatures()["dojo.data.api.Write"]){
if(_27.min.row==_28.min.row&&_27.min.col==_28.min.col){
return;
}
var _29=g.layout.cells,cnt=_27.max.row-_27.min.row+1,r,c,tr,tc,_2a=[],_2b=[];
for(r=_27.min.row,tr=_28.min.row;r<=_27.max.row;++r,++tr){
for(c=_27.min.col,tc=_28.min.col;c<=_27.max.col;++c,++tc){
while(_29[c]&&_29[c].hidden){
++c;
}
while(_29[tc]&&_29[tc].hidden){
++tc;
}
_2a.push({"r":r,"c":c});
_2b.push({"r":tr,"c":tc,"v":_29[c].get(r,g._by_idx[r].item)});
}
}
if(this._hasIdentity(_2a.concat(_2b))){
console.warn("Can not write to identity!");
return;
}
_4.forEach(_2a,function(_2c){
s.setValue(g._by_idx[_2c.r].item,_29[_2c.c].field,"");
});
_4.forEach(_2b,function(_2d){
s.setValue(g._by_idx[_2d.r].item,_29[_2d.c].field,_2d.v);
});
s.save({onComplete:function(){
_5.publish("dojox/grid/rearrange/move/"+g.id,["cell",{"from":_27,"to":_28}]);
}});
}
},copyCells:function(_2e,_2f){
var g=this.grid,s=g.store;
if(s.getFeatures()["dojo.data.api.Write"]){
if(_2e.min.row==_2f.min.row&&_2e.min.col==_2f.min.col){
return;
}
var _30=g.layout.cells,cnt=_2e.max.row-_2e.min.row+1,r,c,tr,tc,_31=[];
for(r=_2e.min.row,tr=_2f.min.row;r<=_2e.max.row;++r,++tr){
for(c=_2e.min.col,tc=_2f.min.col;c<=_2e.max.col;++c,++tc){
while(_30[c]&&_30[c].hidden){
++c;
}
while(_30[tc]&&_30[tc].hidden){
++tc;
}
_31.push({"r":tr,"c":tc,"v":_30[c].get(r,g._by_idx[r].item)});
}
}
if(this._hasIdentity(_31)){
console.warn("Can not write to identity!");
return;
}
_4.forEach(_31,function(_32){
s.setValue(g._by_idx[_32.r].item,_30[_32.c].field,_32.v);
});
s.save({onComplete:function(){
setTimeout(function(){
_5.publish("dojox/grid/rearrange/copy/"+g.id,["cell",{"from":_2e,"to":_2f}]);
},0);
}});
}
},changeCells:function(_33,_34,_35){
var g=this.grid,s=g.store;
if(s.getFeatures()["dojo.data.api.Write"]){
var _36=_33,_37=g.layout.cells,_38=_36.layout.cells,cnt=_34.max.row-_34.min.row+1,r,c,tr,tc,_39=[];
for(r=_34.min.row,tr=_35.min.row;r<=_34.max.row;++r,++tr){
for(c=_34.min.col,tc=_35.min.col;c<=_34.max.col;++c,++tc){
while(_38[c]&&_38[c].hidden){
++c;
}
while(_37[tc]&&_37[tc].hidden){
++tc;
}
_39.push({"r":tr,"c":tc,"v":_38[c].get(r,_36._by_idx[r].item)});
}
}
if(this._hasIdentity(_39)){
console.warn("Can not write to identity!");
return;
}
_4.forEach(_39,function(_3a){
s.setValue(g._by_idx[_3a.r].item,_37[_3a.c].field,_3a.v);
});
s.save({onComplete:function(){
_5.publish("dojox/grid/rearrange/change/"+g.id,["cell",_35]);
}});
}
},clearCells:function(_3b){
var g=this.grid,s=g.store;
if(s.getFeatures()["dojo.data.api.Write"]){
var _3c=g.layout.cells,cnt=_3b.max.row-_3b.min.row+1,r,c,_3d=[];
for(r=_3b.min.row;r<=_3b.max.row;++r){
for(c=_3b.min.col;c<=_3b.max.col;++c){
while(_3c[c]&&_3c[c].hidden){
++c;
}
_3d.push({"r":r,"c":c});
}
}
if(this._hasIdentity(_3d)){
console.warn("Can not write to identity!");
return;
}
_4.forEach(_3d,function(_3e){
s.setValue(g._by_idx[_3e.r].item,_3c[_3e.c].field,"");
});
s.save({onComplete:function(){
_5.publish("dojox/grid/rearrange/change/"+g.id,["cell",_3b]);
}});
}
},insertRows:function(_3f,_40,_41){
try{
var g=this.grid,s=g.store,_42=g.rowCount,_43={},obj={idx:0},_44=[],i,_45=_41<0,_46=this,len=_40.length;
if(_45){
_41=0;
}else{
for(i=_41;i<g.rowCount;++i){
_43[i]=i+len;
}
}
if(s.getFeatures()["dojo.data.api.Write"]){
if(_3f){
var _47=_3f,_48=_47.store,_49,_4a;
if(!_45){
for(i=0;!_49;++i){
_49=g._by_idx[i];
}
_4a=s.getAttributes(_49.item);
}else{
_4a=_4.filter(_4.map(g.layout.cells,function(_4b){
return _4b.field;
}),function(_4c){
return _4c;
});
}
var _4d=[];
_4.forEach(_40,function(_4e,i){
var _4f={};
var _50=_47._by_idx[_4e];
if(_50){
_4.forEach(_4a,function(_51){
_4f[_51]=_48.getValue(_50.item,_51);
});
_4f=_46.args.setIdentifierForNewItem(_4f,s,_42+obj.idx)||_4f;
try{
s.newItem(_4f);
_44.push(_41+i);
_43[_42+obj.idx]=_41+i;
++obj.idx;
}
catch(e){
}
}else{
_4d.push(_4e);
}
});
}else{
if(_40.length&&_2.isObject(_40[0])){
_4.forEach(_40,function(_52,i){
var _53=_46.args.setIdentifierForNewItem(_52,s,_42+obj.idx)||_52;
try{
s.newItem(_53);
_44.push(_41+i);
_43[_42+obj.idx]=_41+i;
++obj.idx;
}
catch(e){
}
});
}else{
return;
}
}
g.layer("rowmap").setMapping(_43);
s.save({onComplete:function(){
g._refresh();
setTimeout(function(){
_5.publish("dojox/grid/rearrange/insert/"+g.id,["row",_44]);
},0);
}});
}
}
catch(e){
}
},removeRows:function(_54){
var g=this.grid;
var s=g.store;
try{
_4.forEach(_4.map(_54,function(_55){
return g._by_idx[_55];
}),function(row){
if(row){
s.deleteItem(row.item);
}
});
s.save({onComplete:function(){
_5.publish("dojox/grid/rearrange/remove/"+g.id,["row",_54]);
}});
}
catch(e){
}
},_getPageInfo:function(){
var _56=this.grid.scroller,_57=_56.page,_58=_56.page,_59=_56.firstVisibleRow,_5a=_56.lastVisibleRow,_5b=_56.rowsPerPage,_5c=_56.pageNodes[0],_5d,_5e,_5f,_60=[];
_4.forEach(_5c,function(_61,_62){
if(!_61){
return;
}
_5f=false;
_5d=_62*_5b;
_5e=(_62+1)*_5b-1;
if(_59>=_5d&&_59<=_5e){
_57=_62;
_5f=true;
}
if(_5a>=_5d&&_5a<=_5e){
_58=_62;
_5f=true;
}
if(!_5f&&(_5d>_5a||_5e<_59)){
_60.push(_62);
}
});
return {topPage:_57,bottomPage:_58,invalidPages:_60};
}});
_6.registerPlugin(_9);
return _9;
});
