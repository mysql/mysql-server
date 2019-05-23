//>>built
define("dijit/tree/ObjectStoreModel",["dojo/_base/array","dojo/aspect","dojo/_base/declare","dojo/_base/lang","dojo/when"],function(_1,_2,_3,_4,_5){
return _3("dijit.tree.ObjectStoreModel",null,{store:null,labelAttr:"name",root:null,query:null,constructor:function(_6){
_4.mixin(this,_6);
this.childrenCache={};
},destroy:function(){
for(var id in this.childrenCache){
this.childrenCache[id].close&&this.childrenCache[id].close();
}
},getRoot:function(_7,_8){
if(this.root){
_7(this.root);
}else{
var _9;
_5(_9=this.store.query(this.query),_4.hitch(this,function(_a){
if(_a.length!=1){
throw new Error("dijit.tree.ObjectStoreModel: root query returned "+_a.length+" items, but must return exactly one");
}
this.root=_a[0];
_7(this.root);
if(_9.observe){
_9.observe(_4.hitch(this,function(_b){
this.onChange(_b);
}),true);
}
}),_8);
}
},mayHaveChildren:function(){
return true;
},getChildren:function(_c,_d,_e){
var id=this.store.getIdentity(_c);
if(this.childrenCache[id]){
_5(this.childrenCache[id],_d,_e);
return;
}
var _f=this.childrenCache[id]=this.store.getChildren(_c);
_5(_f,_d,_e);
if(_f.observe){
_f.observe(_4.hitch(this,function(obj,_10,_11){
this.onChange(obj);
if(_10!=_11){
_5(_f,_4.hitch(this,"onChildrenChange",_c));
}
}),true);
}
},isItem:function(){
return true;
},fetchItemByIdentity:function(_12){
this.store.get(_12.identity).then(_4.hitch(_12.scope,_12.onItem),_4.hitch(_12.scope,_12.onError));
},getIdentity:function(_13){
return this.store.getIdentity(_13);
},getLabel:function(_14){
return _14[this.labelAttr];
},newItem:function(_15,_16,_17,_18){
return this.store.put(_15,{parent:_16,before:_18});
},pasteItem:function(_19,_1a,_1b,_1c,_1d,_1e){
if(!_1c){
var _1f=[].concat(this.childrenCache[this.getIdentity(_1a)]),_20=_1.indexOf(_1f,_19);
_1f.splice(_20,1);
this.onChildrenChange(_1a,_1f);
}
return this.store.put(_19,{overwrite:true,parent:_1b,before:_1e});
},onChange:function(){
},onChildrenChange:function(){
},onDelete:function(){
}});
});
