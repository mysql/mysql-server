//>>built
define("dijit/tree/ObjectStoreModel",["dojo/_base/array","dojo/aspect","dojo/_base/declare","dojo/Deferred","dojo/_base/lang","dojo/when","../Destroyable"],function(_1,_2,_3,_4,_5,_6,_7){
return _3("dijit.tree.ObjectStoreModel",_7,{store:null,labelAttr:"name",labelType:"text",root:null,query:null,constructor:function(_8){
_5.mixin(this,_8);
this.childrenCache={};
},getRoot:function(_9,_a){
if(this.root){
_9(this.root);
}else{
var _b=this.store.query(this.query);
if(_b.then){
this.own(_b);
}
_6(_b,_5.hitch(this,function(_c){
if(_c.length!=1){
throw new Error("dijit.tree.ObjectStoreModel: root query returned "+_c.length+" items, but must return exactly one");
}
this.root=_c[0];
_9(this.root);
if(_b.observe){
_b.observe(_5.hitch(this,function(_d){
this.onChange(_d);
}),true);
}
}),_a);
}
},mayHaveChildren:function(){
return true;
},getChildren:function(_e,_f,_10){
var id=this.store.getIdentity(_e);
if(this.childrenCache[id]){
_6(this.childrenCache[id],_f,_10);
return;
}
var res=this.childrenCache[id]=this.store.getChildren(_e);
if(res.then){
this.own(res);
}
if(res.observe){
this.own(res.observe(_5.hitch(this,function(obj,_11,_12){
this.onChange(obj);
if(_11!=_12){
_6(res,_5.hitch(this,"onChildrenChange",_e));
}
}),true));
}
_6(res,_f,_10);
},isItem:function(){
return true;
},getIdentity:function(_13){
return this.store.getIdentity(_13);
},getLabel:function(_14){
return _14[this.labelAttr];
},newItem:function(_15,_16,_17,_18){
return this.store.put(_15,{parent:_16,before:_18});
},pasteItem:function(_19,_1a,_1b,_1c,_1d,_1e){
var d=new _4();
if(_1a===_1b&&!_1c&&!_1e){
d.resolve(true);
return d;
}
if(_1a&&!_1c){
this.getChildren(_1a,_5.hitch(this,function(_1f){
_1f=[].concat(_1f);
var _20=_1.indexOf(_1f,_19);
_1f.splice(_20,1);
this.onChildrenChange(_1a,_1f);
d.resolve(this.store.put(_19,{overwrite:true,parent:_1b,oldParent:_1a,before:_1e,isCopy:false}));
}));
}else{
d.resolve(this.store.put(_19,{overwrite:true,parent:_1b,oldParent:_1a,before:_1e,isCopy:true}));
}
return d;
},onChange:function(){
},onChildrenChange:function(){
},onDelete:function(){
}});
});
