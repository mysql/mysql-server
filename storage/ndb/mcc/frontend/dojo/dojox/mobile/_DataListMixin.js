//>>built
define("dojox/mobile/_DataListMixin",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dijit/registry","./ListItem"],function(_1,_2,_3,_4,_5,_6){
return _3("dojox.mobile._DataListMixin",null,{store:null,query:null,queryOptions:null,buildRendering:function(){
this.inherited(arguments);
if(!this.store){
return;
}
var _7=this.store;
this.store=null;
this.setStore(_7,this.query,this.queryOptions);
},setStore:function(_8,_9,_a){
if(_8===this.store){
return;
}
this.store=_8;
this.query=_9;
this.queryOptions=_a;
if(_8&&_8.getFeatures()["dojo.data.api.Notification"]){
_1.forEach(this._conn||[],_2.disconnect);
this._conn=[_2.connect(_8,"onSet",this,"onSet"),_2.connect(_8,"onNew",this,"onNew"),_2.connect(_8,"onDelete",this,"onDelete")];
}
this.refresh();
},refresh:function(){
if(!this.store){
return;
}
this.store.fetch({query:this.query,queryOptions:this.queryOptions,onComplete:_4.hitch(this,"onComplete"),onError:_4.hitch(this,"onError")});
},createListItem:function(_b){
var _c={};
var _d=this.store.getLabelAttributes(_b);
var _e=_d?_d[0]:null;
_1.forEach(this.store.getAttributes(_b),function(_f){
if(_f===_e){
_c["label"]=this.store.getLabel(_b);
}else{
_c[_f]=this.store.getValue(_b,_f);
}
},this);
var w=new _6(_c);
_b._widgetId=w.id;
return w;
},generateList:function(_10,_11){
_1.forEach(this.getChildren(),function(_12){
_12.destroyRecursive();
});
_1.forEach(_10,function(_13,_14){
this.addChild(this.createListItem(_13));
},this);
},onComplete:function(_15,_16){
this.generateList(_15,_16);
},onError:function(_17,_18){
},onSet:function(_19,_1a,_1b,_1c){
},onNew:function(_1d,_1e){
this.addChild(this.createListItem(_1d));
},onDelete:function(_1f){
_5.byId(_1f._widgetId).destroyRecursive();
}});
});
