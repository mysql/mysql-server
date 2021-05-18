//>>built
define("dojox/grid/enhanced/plugins/GridSource",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/dnd/Source","./DnD"],function(_1,_2,_3,_4,_5){
var _6=function(_7){
var a=_7[0];
for(var i=1;i<_7.length;++i){
a=a.concat(_7[i]);
}
return a;
};
var _8=_3.getObject("dojox.grid.enhanced.plugins.GridDnDSource");
return _1("dojox.grid.enhanced.plugins.GridSource",_4,{accept:["grid/cells","grid/rows","grid/cols","text"],insertNodesForGrid:false,markupFactory:function(_9,_a){
cls=_3.getObject("dojox.grid.enhanced.plugins.GridSource");
return new cls(_a,_9);
},checkAcceptance:function(_b,_c){
if(_b instanceof _8){
if(_c[0]){
var _d=_b.getItem(_c[0].id);
if(_d&&(_2.indexOf(_d.type,"grid/rows")>=0||_2.indexOf(_d.type,"grid/cells")>=0)&&!_b.dndPlugin._allDnDItemsLoaded()){
return false;
}
}
this.sourcePlugin=_b.dndPlugin;
}
return this.inherited(arguments);
},onDraggingOver:function(){
if(this.sourcePlugin){
this.sourcePlugin._isSource=true;
}
},onDraggingOut:function(){
if(this.sourcePlugin){
this.sourcePlugin._isSource=false;
}
},onDropExternal:function(_e,_f,_10){
if(_e instanceof _8){
var _11=_2.map(_f,function(_12){
return _e.getItem(_12.id).data;
});
var _13=_e.getItem(_f[0].id);
var _14=_13.dndPlugin.grid;
var _15=_13.type[0];
var _16;
try{
switch(_15){
case "grid/cells":
_f[0].innerHTML=this.getCellContent(_14,_11[0].min,_11[0].max)||"";
this.onDropGridCells(_14,_11[0].min,_11[0].max);
break;
case "grid/rows":
_16=_6(_11);
_f[0].innerHTML=this.getRowContent(_14,_16)||"";
this.onDropGridRows(_14,_16);
break;
case "grid/cols":
_16=_6(_11);
_f[0].innerHTML=this.getColumnContent(_14,_16)||"";
this.onDropGridColumns(_14,_16);
break;
}
if(this.insertNodesForGrid){
this.selectNone();
this.insertNodes(true,[_f[0]],this.before,this.current);
}
_13.dndPlugin.onDragOut(!_10);
}
catch(e){
console.warn("GridSource.onDropExternal() error:",e);
}
}else{
this.inherited(arguments);
}
},getCellContent:function(_17,_18,_19){
},getRowContent:function(_1a,_1b){
},getColumnContent:function(_1c,_1d){
},onDropGridCells:function(_1e,_1f,_20){
},onDropGridRows:function(_21,_22){
},onDropGridColumns:function(_23,_24){
}});
});
