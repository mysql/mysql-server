//>>built
define("dojox/mobile/_DataMixin",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/Deferred"],function(_1,_2,_3,_4,_5){
_1.deprecated("dojox/mobile/_DataMixin","Use dojox/mobile/_StoreMixin instead","2.0");
return _3("dojox.mobile._DataMixin",null,{store:null,query:null,queryOptions:null,setStore:function(_6,_7,_8){
if(_6===this.store){
return null;
}
this.store=_6;
this._setQuery(_7,_8);
if(_6&&_6.getFeatures()["dojo.data.api.Notification"]){
_2.forEach(this._conn||[],this.disconnect,this);
this._conn=[this.connect(_6,"onSet","onSet"),this.connect(_6,"onNew","onNew"),this.connect(_6,"onDelete","onDelete"),this.connect(_6,"close","onStoreClose")];
}
return this.refresh();
},setQuery:function(_9,_a){
this._setQuery(_9,_a);
return this.refresh();
},_setQuery:function(_b,_c){
this.query=_b;
this.queryOptions=_c||this.queryOptions;
},refresh:function(){
if(!this.store){
return null;
}
var d=new _5();
var _d=_4.hitch(this,function(_e,_f){
this.onComplete(_e,_f);
d.resolve();
});
var _10=_4.hitch(this,function(_11,_12){
this.onError(_11,_12);
d.resolve();
});
var q=this.query;
this.store.fetch({query:q,queryOptions:this.queryOptions,onComplete:_d,onError:_10,start:q&&q.start,count:q&&q.count});
return d;
}});
});
