//>>built
define("dijit/form/_FormMixin",["dojo/_base/array","dojo/_base/declare","dojo/_base/kernel","dojo/_base/lang","dojo/window"],function(_1,_2,_3,_4,_5){
return _2("dijit.form._FormMixin",null,{state:"",_getDescendantFormWidgets:function(_6){
var _7=[];
_1.forEach(_6||this.getChildren(),function(_8){
if("value" in _8){
_7.push(_8);
}else{
_7=_7.concat(this._getDescendantFormWidgets(_8.getChildren()));
}
},this);
return _7;
},reset:function(){
_1.forEach(this._getDescendantFormWidgets(),function(_9){
if(_9.reset){
_9.reset();
}
});
},validate:function(){
var _a=false;
return _1.every(_1.map(this._getDescendantFormWidgets(),function(_b){
_b._hasBeenBlurred=true;
var _c=_b.disabled||!_b.validate||_b.validate();
if(!_c&&!_a){
_5.scrollIntoView(_b.containerNode||_b.domNode);
_b.focus();
_a=true;
}
return _c;
}),function(_d){
return _d;
});
},setValues:function(_e){
_3.deprecated(this.declaredClass+"::setValues() is deprecated. Use set('value', val) instead.","","2.0");
return this.set("value",_e);
},_setValueAttr:function(_f){
var map={};
_1.forEach(this._getDescendantFormWidgets(),function(_10){
if(!_10.name){
return;
}
var _11=map[_10.name]||(map[_10.name]=[]);
_11.push(_10);
});
for(var _12 in map){
if(!map.hasOwnProperty(_12)){
continue;
}
var _13=map[_12],_14=_4.getObject(_12,false,_f);
if(_14===undefined){
continue;
}
if(!_4.isArray(_14)){
_14=[_14];
}
if(typeof _13[0].checked=="boolean"){
_1.forEach(_13,function(w){
w.set("value",_1.indexOf(_14,w.value)!=-1);
});
}else{
if(_13[0].multiple){
_13[0].set("value",_14);
}else{
_1.forEach(_13,function(w,i){
w.set("value",_14[i]);
});
}
}
}
},getValues:function(){
_3.deprecated(this.declaredClass+"::getValues() is deprecated. Use get('value') instead.","","2.0");
return this.get("value");
},_getValueAttr:function(){
var obj={};
_1.forEach(this._getDescendantFormWidgets(),function(_15){
var _16=_15.name;
if(!_16||_15.disabled){
return;
}
var _17=_15.get("value");
if(typeof _15.checked=="boolean"){
if(/Radio/.test(_15.declaredClass)){
if(_17!==false){
_4.setObject(_16,_17,obj);
}else{
_17=_4.getObject(_16,false,obj);
if(_17===undefined){
_4.setObject(_16,null,obj);
}
}
}else{
var ary=_4.getObject(_16,false,obj);
if(!ary){
ary=[];
_4.setObject(_16,ary,obj);
}
if(_17!==false){
ary.push(_17);
}
}
}else{
var _18=_4.getObject(_16,false,obj);
if(typeof _18!="undefined"){
if(_4.isArray(_18)){
_18.push(_17);
}else{
_4.setObject(_16,[_18,_17],obj);
}
}else{
_4.setObject(_16,_17,obj);
}
}
});
return obj;
},isValid:function(){
return this.state=="";
},onValidStateChange:function(){
},_getState:function(){
var _19=_1.map(this._descendants,function(w){
return w.get("state")||"";
});
return _1.indexOf(_19,"Error")>=0?"Error":_1.indexOf(_19,"Incomplete")>=0?"Incomplete":"";
},disconnectChildren:function(){
_1.forEach(this._childConnections||[],_4.hitch(this,"disconnect"));
_1.forEach(this._childWatches||[],function(w){
w.unwatch();
});
},connectChildren:function(_1a){
var _1b=this;
this.disconnectChildren();
this._descendants=this._getDescendantFormWidgets();
var set=_1a?function(_1c,val){
_1b[_1c]=val;
}:_4.hitch(this,"_set");
set("value",this.get("value"));
set("state",this._getState());
var _1d=(this._childConnections=[]),_1e=(this._childWatches=[]);
_1.forEach(_1.filter(this._descendants,function(_1f){
return _1f.validate;
}),function(_20){
_1.forEach(["state","disabled"],function(_21){
_1e.push(_20.watch(_21,function(){
_1b.set("state",_1b._getState());
}));
});
});
var _22=function(){
if(_1b._onChangeDelayTimer){
clearTimeout(_1b._onChangeDelayTimer);
}
_1b._onChangeDelayTimer=setTimeout(function(){
delete _1b._onChangeDelayTimer;
_1b._set("value",_1b.get("value"));
},10);
};
_1.forEach(_1.filter(this._descendants,function(_23){
return _23.onChange;
}),function(_24){
_1d.push(_1b.connect(_24,"onChange",_22));
_1e.push(_24.watch("disabled",_22));
});
},startup:function(){
this.inherited(arguments);
this.connectChildren(true);
this.watch("state",function(_25,_26,_27){
this.onValidStateChange(_27=="");
});
},destroy:function(){
this.disconnectChildren();
this.inherited(arguments);
}});
});
