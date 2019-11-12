//>>built
define("dojox/mobile/_DataMixin",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/Deferred"],function(_1,_2,_3,_4){
return _2("dojox.mobile._DataMixin",null,{store:null,query:null,queryOptions:null,setStore:function(_5,_6,_7){
if(_5===this.store){
return null;
}
this.store=_5;
this._setQuery(_6,_7);
if(_5&&_5.getFeatures()["dojo.data.api.Notification"]){
_1.forEach(this._conn||[],this.disconnect,this);
this._conn=[this.connect(_5,"onSet","onSet"),this.connect(_5,"onNew","onNew"),this.connect(_5,"onDelete","onDelete"),this.connect(_5,"close","onStoreClose")];
}
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
var d=new _4();
var _c=_3.hitch(this,function(_d,_e){
this.onComplete(_d,_e);
d.resolve();
});
var _f=_3.hitch(this,function(_10,_11){
this.onError(_10,_11);
d.resolve();
});
var q=this.query;
this.store.fetch({query:q,queryOptions:this.queryOptions,onComplete:_c,onError:_f,start:q&&q.start,count:q&&q.count});
return d;
}});
});
