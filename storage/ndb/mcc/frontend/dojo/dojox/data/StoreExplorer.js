//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/grid/DataGrid,dojox/data/ItemExplorer,dijit/layout/BorderContainer,dijit/layout/ContentPane"],function(_1,_2,_3){
_2.provide("dojox.data.StoreExplorer");
_2.require("dojox.grid.DataGrid");
_2.require("dojox.data.ItemExplorer");
_2.require("dijit.layout.BorderContainer");
_2.require("dijit.layout.ContentPane");
_2.declare("dojox.data.StoreExplorer",_1.layout.BorderContainer,{constructor:function(_4){
_2.mixin(this,_4);
},store:null,columnWidth:"",stringQueries:false,showAllColumns:false,postCreate:function(){
var _5=this;
this.inherited(arguments);
var _6=new _1.layout.ContentPane({region:"top"}).placeAt(this);
function _7(_8,_9){
var _a=new _1.form.Button({label:_8});
_6.containerNode.appendChild(_a.domNode);
_a.onClick=_9;
return _a;
};
var _b=_6.containerNode.appendChild(document.createElement("span"));
_b.innerHTML="Enter query: &nbsp;";
_b.id="queryText";
var _c=_6.containerNode.appendChild(document.createElement("input"));
_c.type="text";
_c.id="queryTextBox";
_7("Query",function(){
var _d=_c.value;
_5.setQuery(_5.stringQueries?_d:_2.fromJson(_d));
});
_6.containerNode.appendChild(document.createElement("span")).innerHTML="&nbsp;&nbsp;&nbsp;";
var _e=_7("Create New",_2.hitch(this,"createNew"));
var _f=_7("Delete",function(){
var _10=_11.selection.getSelected();
for(var i=0;i<_10.length;i++){
_5.store.deleteItem(_10[i]);
}
});
this.setItemName=function(_12){
_e.attr("label","<img style='width:12px; height:12px' src='"+_2.moduleUrl("dijit.themes.tundra.images","dndCopy.png")+"' /> Create New "+_12);
_f.attr("label","Delete "+_12);
};
_7("Save",function(){
_5.store.save({onError:function(_13){
alert(_13);
}});
_5.tree.refreshItem();
});
_7("Revert",function(){
_5.store.revert();
});
_7("Add Column",function(){
var _14=prompt("Enter column name:","property");
if(_14){
_5.gridLayout.push({field:_14,name:_14,formatter:_2.hitch(_5,"_formatCell"),editable:true});
_5.grid.attr("structure",_5.gridLayout);
}
});
var _15=new _1.layout.ContentPane({region:"center"}).placeAt(this);
var _11=this.grid=new _3.grid.DataGrid({store:this.store});
_15.attr("content",_11);
_11.canEdit=function(_16,_17){
var _18=this._copyAttr(_17,_16.field);
return !(_18&&typeof _18=="object")||_18 instanceof Date;
};
var _19=new _1.layout.ContentPane({region:"trailing",splitter:true,style:"width: 300px"}).placeAt(this);
var _1a=this.tree=new _3.data.ItemExplorer({store:this.store});
_19.attr("content",_1a);
_2.connect(_11,"onCellClick",function(){
var _1b=_11.selection.getSelected()[0];
_1a.setItem(_1b);
});
this.gridOnFetchComplete=_11._onFetchComplete;
this.setStore(this.store);
},setQuery:function(_1c,_1d){
this.grid.setQuery(_1c,_1d);
},_formatCell:function(_1e){
if(this.store.isItem(_1e)){
return this.store.getLabel(_1e)||this.store.getIdentity(_1e);
}
return _1e;
},setStore:function(_1f){
this.store=_1f;
var _20=this;
var _21=this.grid;
_21._pending_requests[0]=false;
function _22(_23){
return _20._formatCell(_23);
};
var _24=this.gridOnFetchComplete;
_21._onFetchComplete=function(_25,req){
var _26=_20.gridLayout=[];
var _27,key,_28,i,j,k,_29=_1f.getIdentityAttributes();
for(i=0;i<_29.length;i++){
key=_29[i];
_26.push({field:key,name:key,_score:100,formatter:_22,editable:false});
}
for(i=0;_28=_25[i++];){
var _2a=_1f.getAttributes(_28);
for(k=0;key=_2a[k++];){
var _2b=false;
for(j=0;_27=_26[j++];){
if(_27.field==key){
_27._score++;
_2b=true;
break;
}
}
if(!_2b){
_26.push({field:key,name:key,_score:1,formatter:_22,styles:"white-space:nowrap; ",editable:true});
}
}
}
_26=_26.sort(function(a,b){
return b._score-a._score;
});
if(!_20.showAllColumns){
for(j=0;_27=_26[j];j++){
if(_27._score<_25.length/40*j){
_26.splice(j,_26.length-j);
break;
}
}
}
for(j=0;_27=_26[j++];){
_27.width=_20.columnWidth||Math.round(100/_26.length)+"%";
}
_21._onFetchComplete=_24;
_21.attr("structure",_26);
var _2c=_24.apply(this,arguments);
};
_21.setStore(_1f);
this.queryOptions={cache:true};
this.tree.setStore(_1f);
},createNew:function(){
var _2d=prompt("Enter any properties (in JSON literal form) to put in the new item (passed to the newItem constructor):","{ }");
if(_2d){
try{
this.store.newItem(_2.fromJson(_2d));
}
catch(e){
alert(e);
}
}
}});
});
