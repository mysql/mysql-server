//>>built
define("dojox/grid/LazyTreeGridStoreModel",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dijit/tree/ForestStoreModel"],function(_1,_2,_3,_4){
return _1("dojox.grid.LazyTreeGridStoreModel",_4,{serverStore:false,constructor:function(_5){
this.serverStore=!!_5.serverStore;
},mayHaveChildren:function(_6){
var _7=null;
return _2.some(this.childrenAttrs,function(_8){
_7=this.store.getValue(_6,_8);
if(_3.isString(_7)){
return parseInt(_7,10)>0||_7.toLowerCase()==="true"?true:false;
}else{
if(typeof _7=="number"){
return _7>0;
}else{
if(typeof _7=="boolean"){
return _7;
}else{
if(this.store.isItem(_7)){
_7=this.store.getValues(_6,_8);
return _3.isArray(_7)?_7.length>0:false;
}else{
return false;
}
}
}
}
},this);
},getChildren:function(_9,_a,_b,_c){
if(_c){
var _d=_c.start||0,_e=_c.count,_f=_c.parentId,_10=_c.sort;
if(_9===this.root){
this.root.size=0;
this.store.fetch({start:_d,count:_e,sort:_10,query:this.query,onBegin:_3.hitch(this,function(_11){
this.root.size=_11;
}),onComplete:_3.hitch(this,function(_12){
_a(_12,_c,this.root.size);
}),onError:_b});
}else{
var _13=this.store;
if(!_13.isItemLoaded(_9)){
var _14=_3.hitch(this,arguments.callee);
_13.loadItem({item:_9,onItem:function(_15){
_14(_15,_a,_b,_c);
},onError:_b});
return;
}
if(this.serverStore&&!this._isChildrenLoaded(_9)){
this.childrenSize=0;
this.store.fetch({start:_d,count:_e,sort:_10,query:_3.mixin({parentId:_f},this.query||{}),onBegin:_3.hitch(this,function(_16){
this.childrenSize=_16;
}),onComplete:_3.hitch(this,function(_17){
_a(_17,_c,this.childrenSize);
}),onError:_b});
}else{
this.inherited(arguments);
}
}
}else{
this.inherited(arguments);
}
},_isChildrenLoaded:function(_18){
var _19=null;
return _2.every(this.childrenAttrs,function(_1a){
_19=this.store.getValues(_18,_1a);
return _2.every(_19,function(c){
return this.store.isItemLoaded(c);
},this);
},this);
},onNewItem:function(_1b,_1c){
},onDeleteItem:function(_1d){
}});
});
