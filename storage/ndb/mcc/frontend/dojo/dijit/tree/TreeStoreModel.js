//>>built
define("dijit/tree/TreeStoreModel",["dojo/_base/array","dojo/aspect","dojo/_base/declare","dojo/_base/json","dojo/_base/lang"],function(_1,_2,_3,_4,_5){
return _3("dijit.tree.TreeStoreModel",null,{store:null,childrenAttrs:["children"],newItemIdAttr:"id",labelAttr:"",root:null,query:null,deferItemLoadingUntilExpand:false,constructor:function(_6){
_5.mixin(this,_6);
this.connects=[];
var _7=this.store;
if(!_7.getFeatures()["dojo.data.api.Identity"]){
throw new Error("dijit.Tree: store must support dojo.data.Identity");
}
if(_7.getFeatures()["dojo.data.api.Notification"]){
this.connects=this.connects.concat([_2.after(_7,"onNew",_5.hitch(this,"onNewItem"),true),_2.after(_7,"onDelete",_5.hitch(this,"onDeleteItem"),true),_2.after(_7,"onSet",_5.hitch(this,"onSetItem"),true)]);
}
},destroy:function(){
var h;
while(h=this.connects.pop()){
h.remove();
}
},getRoot:function(_8,_9){
if(this.root){
_8(this.root);
}else{
this.store.fetch({query:this.query,onComplete:_5.hitch(this,function(_a){
if(_a.length!=1){
throw new Error(this.declaredClass+": query "+_4.stringify(this.query)+" returned "+_a.length+" items, but must return exactly one item");
}
this.root=_a[0];
_8(this.root);
}),onError:_9});
}
},mayHaveChildren:function(_b){
return _1.some(this.childrenAttrs,function(_c){
return this.store.hasAttribute(_b,_c);
},this);
},getChildren:function(_d,_e,_f){
var _10=this.store;
if(!_10.isItemLoaded(_d)){
var _11=_5.hitch(this,arguments.callee);
_10.loadItem({item:_d,onItem:function(_12){
_11(_12,_e,_f);
},onError:_f});
return;
}
var _13=[];
for(var i=0;i<this.childrenAttrs.length;i++){
var _14=_10.getValues(_d,this.childrenAttrs[i]);
_13=_13.concat(_14);
}
var _15=0;
if(!this.deferItemLoadingUntilExpand){
_1.forEach(_13,function(_16){
if(!_10.isItemLoaded(_16)){
_15++;
}
});
}
if(_15==0){
_e(_13);
}else{
_1.forEach(_13,function(_17,idx){
if(!_10.isItemLoaded(_17)){
_10.loadItem({item:_17,onItem:function(_18){
_13[idx]=_18;
if(--_15==0){
_e(_13);
}
},onError:_f});
}
});
}
},isItem:function(_19){
return this.store.isItem(_19);
},fetchItemByIdentity:function(_1a){
this.store.fetchItemByIdentity(_1a);
},getIdentity:function(_1b){
return this.store.getIdentity(_1b);
},getLabel:function(_1c){
if(this.labelAttr){
return this.store.getValue(_1c,this.labelAttr);
}else{
return this.store.getLabel(_1c);
}
},newItem:function(_1d,_1e,_1f){
var _20={parent:_1e,attribute:this.childrenAttrs[0]},_21;
if(this.newItemIdAttr&&_1d[this.newItemIdAttr]){
this.fetchItemByIdentity({identity:_1d[this.newItemIdAttr],scope:this,onItem:function(_22){
if(_22){
this.pasteItem(_22,null,_1e,true,_1f);
}else{
_21=this.store.newItem(_1d,_20);
if(_21&&(_1f!=undefined)){
this.pasteItem(_21,_1e,_1e,false,_1f);
}
}
}});
}else{
_21=this.store.newItem(_1d,_20);
if(_21&&(_1f!=undefined)){
this.pasteItem(_21,_1e,_1e,false,_1f);
}
}
},pasteItem:function(_23,_24,_25,_26,_27){
var _28=this.store,_29=this.childrenAttrs[0];
if(_24){
_1.forEach(this.childrenAttrs,function(_2a){
if(_28.containsValue(_24,_2a,_23)){
if(!_26){
var _2b=_1.filter(_28.getValues(_24,_2a),function(x){
return x!=_23;
});
_28.setValues(_24,_2a,_2b);
}
_29=_2a;
}
});
}
if(_25){
if(typeof _27=="number"){
var _2c=_28.getValues(_25,_29).slice();
_2c.splice(_27,0,_23);
_28.setValues(_25,_29,_2c);
}else{
_28.setValues(_25,_29,_28.getValues(_25,_29).concat(_23));
}
}
},onChange:function(){
},onChildrenChange:function(){
},onDelete:function(){
},onNewItem:function(_2d,_2e){
if(!_2e){
return;
}
this.getChildren(_2e.item,_5.hitch(this,function(_2f){
this.onChildrenChange(_2e.item,_2f);
}));
},onDeleteItem:function(_30){
this.onDelete(_30);
},onSetItem:function(_31,_32){
if(_1.indexOf(this.childrenAttrs,_32)!=-1){
this.getChildren(_31,_5.hitch(this,function(_33){
this.onChildrenChange(_31,_33);
}));
}else{
this.onChange(_31);
}
}});
});
