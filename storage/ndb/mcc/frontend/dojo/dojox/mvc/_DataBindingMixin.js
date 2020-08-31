//>>built
define("dojox/mvc/_DataBindingMixin",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/Stateful","dijit/registry"],function(_1,_2,_3,_4,_5,_6){
_1.deprecated("dojox.mvc._DataBindingMixin","Use dojox/mvc/at for data binding.");
return _4("dojox.mvc._DataBindingMixin",null,{ref:null,isValid:function(){
var _7=this.get("valid");
return typeof _7!="undefined"?_7:this.get("binding")?this.get("binding").get("valid"):true;
},_dbstartup:function(){
if(this._databound){
return;
}
this._unwatchArray(this._viewWatchHandles);
this._viewWatchHandles=[this.watch("ref",function(_8,_9,_a){
if(this._databound&&_9!==_a){
this._setupBinding();
}
}),this.watch("value",function(_b,_c,_d){
if(this._databound){
var _e=this.get("binding");
if(_e){
if(!((_d&&_c)&&(_c.valueOf()===_d.valueOf()))){
_e.set("value",_d);
}
}
}
})];
this._beingBound=true;
this._setupBinding();
delete this._beingBound;
this._databound=true;
},_setupBinding:function(_f){
if(!this.ref){
return;
}
var ref=this.ref,pw,pb,_10;
if(ref&&_2.isFunction(ref.toPlainObject)){
_10=ref;
}else{
if(/^\s*expr\s*:\s*/.test(ref)){
ref=ref.replace(/^\s*expr\s*:\s*/,"");
_10=_2.getObject(ref);
}else{
if(/^\s*rel\s*:\s*/.test(ref)){
ref=ref.replace(/^\s*rel\s*:\s*/,"");
_f=_f||this._getParentBindingFromDOM();
if(_f){
_10=_2.getObject(""+ref,false,_f);
}
}else{
if(/^\s*widget\s*:\s*/.test(ref)){
ref=ref.replace(/^\s*widget\s*:\s*/,"");
var _11=ref.split(".");
if(_11.length==1){
_10=_6.byId(ref).get("binding");
}else{
pb=_6.byId(_11.shift()).get("binding");
_10=_2.getObject(_11.join("."),false,pb);
}
}else{
_f=_f||this._getParentBindingFromDOM();
if(_f){
_10=_2.getObject(""+ref,false,_f);
}else{
try{
var b=_2.getObject(""+ref)||{};
if(_2.isFunction(b.set)&&_2.isFunction(b.watch)){
_10=b;
}
}
catch(err){
if(ref.indexOf("${")==-1){
console.warn("dojox/mvc/_DataBindingMixin: '"+this.domNode+"' widget with illegal ref not evaluating to a dojo/Stateful node: '"+ref+"'");
}
}
}
}
}
}
}
if(_10){
if(_2.isFunction(_10.toPlainObject)){
this.binding=_10;
if(this[this._relTargetProp||"target"]!==_10){
this.set(this._relTargetProp||"target",_10);
}
this._updateBinding("binding",null,_10);
}else{
console.warn("dojox/mvc/_DataBindingMixin: '"+this.domNode+"' widget with illegal ref not evaluating to a dojo/Stateful node: '"+ref+"'");
}
}
},_isEqual:function(one,_12){
return one===_12||isNaN(one)&&typeof one==="number"&&isNaN(_12)&&typeof _12==="number";
},_updateBinding:function(_13,old,_14){
this._unwatchArray(this._modelWatchHandles);
var _15=this.get("binding");
if(_15&&_2.isFunction(_15.watch)){
var _16=this;
this._modelWatchHandles=[_15.watch("value",function(_17,old,_18){
if(_16._isEqual(old,_18)){
return;
}
if(_16._isEqual(_16.get("value"),_18)){
return;
}
_16.set("value",_18);
}),_15.watch("valid",function(_19,old,_1a){
_16._updateProperty(_19,old,_1a,true);
if(_1a!==_16.get(_19)){
if(_16.validate&&_2.isFunction(_16.validate)){
_16.validate();
}
}
}),_15.watch("required",function(_1b,old,_1c){
_16._updateProperty(_1b,old,_1c,false,_1b,_1c);
}),_15.watch("readOnly",function(_1d,old,_1e){
_16._updateProperty(_1d,old,_1e,false,_1d,_1e);
}),_15.watch("relevant",function(_1f,old,_20){
_16._updateProperty(_1f,old,_20,false,"disabled",!_20);
})];
var val=_15.get("value");
if(val!=null){
this.set("value",val);
}
}
this._updateChildBindings();
},_updateProperty:function(_21,old,_22,_23,_24,_25){
if(old===_22){
return;
}
if(_22===null&&_23!==undefined){
_22=_23;
}
if(_22!==this.get("binding").get(_21)){
this.get("binding").set(_21,_22);
}
if(_24){
this.set(_24,_25);
}
},_updateChildBindings:function(_26){
var _27=this.get("binding")||_26;
if(_27&&!this._beingBound){
_3.forEach(_6.findWidgets(this.domNode),function(_28){
if(_28.ref&&_28._setupBinding){
_28._setupBinding(_27);
}else{
_28._updateChildBindings(_27);
}
});
}
},_getParentBindingFromDOM:function(){
var pn=this.domNode.parentNode,pw,pb;
while(pn){
pw=_6.getEnclosingWidget(pn);
if(pw){
pb=pw.get("binding");
if(pb&&_2.isFunction(pb.toPlainObject)){
break;
}
}
pn=pw?pw.domNode.parentNode:null;
}
return pb;
},_unwatchArray:function(_29){
_3.forEach(_29,function(h){
h.unwatch();
});
}});
});
