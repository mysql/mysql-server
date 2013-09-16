//>>built
define("dojox/data/CssClassStore",["dojo/_base/declare","dojox/data/CssRuleStore"],function(_1,_2){
return _1("dojox.data.CssClassStore",_2,{_labelAttribute:"class",_idAttribute:"class",_cName:"dojox.data.CssClassStore",getFeatures:function(){
return {"dojo.data.api.Read":true,"dojo.data.api.Identity":true};
},getAttributes:function(_3){
this._assertIsItem(_3);
return ["class","classSans"];
},getValue:function(_4,_5,_6){
var _7=this.getValues(_4,_5);
if(_7&&_7.length>0){
return _7[0];
}
return _6;
},getValues:function(_8,_9){
this._assertIsItem(_8);
this._assertIsAttribute(_9);
var _a=[];
if(_9==="class"){
_a=[_8.className];
}else{
if(_9==="classSans"){
_a=[_8.className.replace(/\./g,"")];
}
}
return _a;
},_handleRule:function(_b,_c,_d){
var _e={};
var s=_b["selectorText"].split(" ");
for(var j=0;j<s.length;j++){
var _f=s[j];
var _10=_f.indexOf(".");
if(_f&&_f.length>0&&_10!==-1){
var _11=_f.indexOf(",")||_f.indexOf("[");
_f=_f.substring(_10,((_11!==-1&&_11>_10)?_11:_f.length));
_e[_f]=true;
}
}
for(var key in _e){
if(!this._allItems[key]){
var _12={};
_12.className=key;
_12[this._storeRef]=this;
this._allItems[key]=_12;
}
}
},_handleReturn:function(){
var _13=[];
var _14={};
for(var i in this._allItems){
_14[i]=this._allItems[i];
}
var _15;
while(this._pending.length){
_15=this._pending.pop();
_15.request._items=_14;
_13.push(_15);
}
while(_13.length){
_15=_13.pop();
if(_15.fetch){
this._handleFetchReturn(_15.request);
}else{
this._handleFetchByIdentityReturn(_15.request);
}
}
},_handleFetchByIdentityReturn:function(_16){
var _17=_16._items;
var _18=_17[_16.identity];
if(!this.isItem(_18)){
_18=null;
}
if(_16.onItem){
var _19=_16.scope||dojo.global;
_16.onItem.call(_19,_18);
}
},getIdentity:function(_1a){
this._assertIsItem(_1a);
return this.getValue(_1a,this._idAttribute);
},getIdentityAttributes:function(_1b){
this._assertIsItem(_1b);
return [this._idAttribute];
},fetchItemByIdentity:function(_1c){
_1c=_1c||{};
if(!_1c.store){
_1c.store=this;
}
if(this._pending&&this._pending.length>0){
this._pending.push({request:_1c});
}else{
this._pending=[{request:_1c}];
this._fetch(_1c);
}
return _1c;
}});
});
