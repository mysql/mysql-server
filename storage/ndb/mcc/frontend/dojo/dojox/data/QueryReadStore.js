//>>built
define("dojox/data/QueryReadStore",["dojo","dojox","dojo/data/util/sorter","dojo/string"],function(_1,_2){
return _1.declare("dojox.data.QueryReadStore",null,{url:"",requestMethod:"get",_className:"dojox.data.QueryReadStore",_items:[],_lastServerQuery:null,_numRows:-1,lastRequestHash:null,doClientPaging:false,doClientSorting:false,_itemsByIdentity:null,_identifier:null,_features:{"dojo.data.api.Read":true,"dojo.data.api.Identity":true},_labelAttr:"label",constructor:function(_3){
_1.mixin(this,_3);
},getValue:function(_4,_5,_6){
this._assertIsItem(_4);
if(!_1.isString(_5)){
throw new Error(this._className+".getValue(): Invalid attribute, string expected!");
}
if(!this.hasAttribute(_4,_5)){
if(_6){
return _6;
}
}
return _4.i[_5];
},getValues:function(_7,_8){
this._assertIsItem(_7);
var _9=[];
if(this.hasAttribute(_7,_8)){
_9.push(_7.i[_8]);
}
return _9;
},getAttributes:function(_a){
this._assertIsItem(_a);
var _b=[];
for(var i in _a.i){
_b.push(i);
}
return _b;
},hasAttribute:function(_c,_d){
return this.isItem(_c)&&typeof _c.i[_d]!="undefined";
},containsValue:function(_e,_f,_10){
var _11=this.getValues(_e,_f);
var len=_11.length;
for(var i=0;i<len;i++){
if(_11[i]==_10){
return true;
}
}
return false;
},isItem:function(_12){
if(_12){
return typeof _12.r!="undefined"&&_12.r==this;
}
return false;
},isItemLoaded:function(_13){
return this.isItem(_13);
},loadItem:function(_14){
if(this.isItemLoaded(_14.item)){
return;
}
},fetch:function(_15){
_15=_15||{};
if(!_15.store){
_15.store=this;
}
var _16=this;
var _17=function(_18,_19){
if(_19.onError){
var _1a=_19.scope||_1.global;
_19.onError.call(_1a,_18,_19);
}
};
var _1b=function(_1c,_1d,_1e){
var _1f=_1d.abort||null;
var _20=false;
var _21=_1d.start?_1d.start:0;
if(_16.doClientPaging==false){
_21=0;
}
var _22=_1d.count?(_21+_1d.count):_1c.length;
_1d.abort=function(){
_20=true;
if(_1f){
_1f.call(_1d);
}
};
var _23=_1d.scope||_1.global;
if(!_1d.store){
_1d.store=_16;
}
if(_1d.onBegin){
_1d.onBegin.call(_23,_1e,_1d);
}
if(_1d.sort&&_16.doClientSorting){
_1c.sort(_1.data.util.sorter.createSortFunction(_1d.sort,_16));
}
if(_1d.onItem){
for(var i=_21;(i<_1c.length)&&(i<_22);++i){
var _24=_1c[i];
if(!_20){
_1d.onItem.call(_23,_24,_1d);
}
}
}
if(_1d.onComplete&&!_20){
var _25=null;
if(!_1d.onItem){
_25=_1c.slice(_21,_22);
}
_1d.onComplete.call(_23,_25,_1d);
}
};
this._fetchItems(_15,_1b,_17);
return _15;
},getFeatures:function(){
return this._features;
},close:function(_26){
},getLabel:function(_27){
if(this._labelAttr&&this.isItem(_27)){
return this.getValue(_27,this._labelAttr);
}
return undefined;
},getLabelAttributes:function(_28){
if(this._labelAttr){
return [this._labelAttr];
}
return null;
},_xhrFetchHandler:function(_29,_2a,_2b,_2c){
_29=this._filterResponse(_29);
if(_29.label){
this._labelAttr=_29.label;
}
var _2d=_29.numRows||-1;
this._items=[];
_1.forEach(_29.items,function(e){
this._items.push({i:e,r:this});
},this);
var _2e=_29.identifier;
this._itemsByIdentity={};
if(_2e){
this._identifier=_2e;
var i;
for(i=0;i<this._items.length;++i){
var _2f=this._items[i].i;
var _30=_2f[_2e];
if(!this._itemsByIdentity[_30]){
this._itemsByIdentity[_30]=_2f;
}else{
throw new Error(this._className+":  The json data as specified by: ["+this.url+"] is malformed.  Items within the list have identifier: ["+_2e+"].  Value collided: ["+_30+"]");
}
}
}else{
this._identifier=Number;
for(i=0;i<this._items.length;++i){
this._items[i].n=i;
}
}
_2d=this._numRows=(_2d===-1)?this._items.length:_2d;
_2b(this._items,_2a,_2d);
this._numRows=_2d;
},_fetchItems:function(_31,_32,_33){
var _34=_31.serverQuery||_31.query||{};
if(!this.doClientPaging){
_34.start=_31.start||0;
if(_31.count){
_34.count=_31.count;
}
}
if(!this.doClientSorting&&_31.sort){
var _35=[];
_1.forEach(_31.sort,function(_36){
if(_36&&_36.attribute){
_35.push((_36.descending?"-":"")+_36.attribute);
}
});
_34.sort=_35.join(",");
}
if(this.doClientPaging&&this._lastServerQuery!==null&&_1.toJson(_34)==_1.toJson(this._lastServerQuery)){
this._numRows=(this._numRows===-1)?this._items.length:this._numRows;
_32(this._items,_31,this._numRows);
}else{
var _37=this.requestMethod.toLowerCase()=="post"?_1.xhrPost:_1.xhrGet;
var _38=_37({url:this.url,handleAs:"json-comment-optional",content:_34,failOk:true});
_31.abort=function(){
_38.cancel();
};
_38.addCallback(_1.hitch(this,function(_39){
this._xhrFetchHandler(_39,_31,_32,_33);
}));
_38.addErrback(function(_3a){
_33(_3a,_31);
});
this.lastRequestHash=new Date().getTime()+"-"+String(Math.random()).substring(2);
this._lastServerQuery=_1.mixin({},_34);
}
},_filterResponse:function(_3b){
return _3b;
},_assertIsItem:function(_3c){
if(!this.isItem(_3c)){
throw new Error(this._className+": Invalid item argument.");
}
},_assertIsAttribute:function(_3d){
if(typeof _3d!=="string"){
throw new Error(this._className+": Invalid attribute argument ('"+_3d+"').");
}
},fetchItemByIdentity:function(_3e){
if(this._itemsByIdentity){
var _3f=this._itemsByIdentity[_3e.identity];
if(!(_3f===undefined)){
if(_3e.onItem){
var _40=_3e.scope?_3e.scope:_1.global;
_3e.onItem.call(_40,{i:_3f,r:this});
}
return;
}
}
var _41=function(_42,_43){
var _44=_3e.scope?_3e.scope:_1.global;
if(_3e.onError){
_3e.onError.call(_44,_42);
}
};
var _45=function(_46,_47){
var _48=_3e.scope?_3e.scope:_1.global;
try{
var _49=null;
if(_46&&_46.length==1){
_49=_46[0];
}
if(_3e.onItem){
_3e.onItem.call(_48,_49);
}
}
catch(error){
if(_3e.onError){
_3e.onError.call(_48,error);
}
}
};
var _4a={serverQuery:{id:_3e.identity}};
this._fetchItems(_4a,_45,_41);
},getIdentity:function(_4b){
var _4c=null;
if(this._identifier===Number){
_4c=_4b.n;
}else{
_4c=_4b.i[this._identifier];
}
return _4c;
},getIdentityAttributes:function(_4d){
return [this._identifier];
}});
});
