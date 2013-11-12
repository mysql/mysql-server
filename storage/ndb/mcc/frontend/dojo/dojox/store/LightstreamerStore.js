//>>built
define("dojox/store/LightstreamerStore",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/Deferred","dojo/store/util/QueryResults"],function(_1){
_1.getObject("store",true,dojox);
var _2=0;
function _3(id,_4,_5,o){
o=o||{};
_1.forEach(_5,function(_6){
o[_6]=_4.getNewValue(_6);
});
if(!("id" in o)){
o["id"]=id;
}
return o;
};
_1.declare("dojox.store.LightstreamerStore",[],{_index:{},pushPage:null,group:[],schema:[],constructor:function(_7,_8,_9,_a){
this.pushPage=_7;
this.group=_8;
this.schema=_9;
this.dataAdapter=_a||"DEFAULT";
},query:function(_b,_c){
_c=_c||{};
var _d=new _1.Deferred(),_e,_f=[],_10=this,id="store-"+_2++,_11=this.pushPage,_12=new NonVisualTable(this.group,this.schema,_b);
if(!("dataAdapter" in _c)&&this.dataAdapter){
_12.setDataAdapter(this.dataAdapter);
}
for(var _13 in _c){
var _14="set"+_13.charAt(0).toUpperCase()+_13.slice(1);
if(_14 in _12&&_1.isFunction(_12[_14])){
_12[_14][(_1.isArray(_c[_13])?"apply":"call")](_12,_c[_13]);
}
}
_12.onItemUpdate=function(id,_15){
var obj=_3(id,_15,_10.schema,_10._index[id]);
var _16;
if(!_10._index[id]){
_16=true;
_10._index[id]=obj;
if(!_e){
_f.push(obj);
}
}
_12[_e?"onPostSnapShot":"onPreSnapShot"](obj,_16);
};
if(_b=="MERGE"||_c.snapshotRequired===false){
_e=true;
_d.resolve(_f);
}else{
_12.onEndOfSnapshot=function(){
_e=true;
_d.resolve(_f);
};
}
_12.onPreSnapShot=function(){
};
_12.onPostSnapShot=function(){
};
_d=_1.store.util.QueryResults(_d);
var _17;
_d.forEach=function(_18){
_17=_1.connect(_12,"onPreSnapShot",_18);
};
var _19;
_d.observe=function(_1a){
_19=_1.connect(_12,"onPostSnapShot",function(_1b,_1c){
_1a.call(_d,_1b,_1c?-1:undefined);
});
};
_d.close=function(){
if(_17){
_1.disconnect(_17);
}
if(_19){
_1.disconnect(_19);
}
_11.removeTable(id);
_12=null;
};
_11.addTable(_12,id);
return _d;
},get:function(id){
return this._index[id];
}});
return dojox.store.LightstreamerStore;
});
