//>>built
define("dojox/mobile/_StoreMixin",["dojo/_base/Deferred","dojo/_base/declare"],function(_1,_2){
return _2("dojox.mobile._StoreMixin",null,{store:null,query:null,queryOptions:null,labelProperty:"label",childrenProperty:"children",setStore:function(_3,_4,_5){
if(_3===this.store){
return null;
}
if(_3){
_3.getValue=function(_6,_7){
return _6[_7];
};
}
this.store=_3;
this._setQuery(_4,_5);
return this.refresh();
},setQuery:function(_8,_9){
this._setQuery(_8,_9);
return this.refresh();
},_setQuery:function(_a,_b){
this.query=_a;
this.queryOptions=_b||this.queryOptions;
},refresh:function(){
if(!this.store){
return null;
}
var _c=this;
var _d=this.store.query(this.query,this.queryOptions);
_1.when(_d,function(_e){
if(_e.items){
_e=_e.items;
}
if(_d.observe){
_d.observe(function(_f,_10,_11){
if(_10>-1){
_c.onDelete(_f,_10);
}else{
if(_11>-1){
_c.onUpdate(_f,_11);
}
}
});
}
_c.onComplete(_e);
},function(_12){
_c.onError(_12);
});
return _d;
}});
});
