//>>built
define("dojox/mvc/_DataBindingMixin",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dijit/registry"],function(_1,_2,_3,_4){
return _3("dojox.mvc._DataBindingMixin",null,{ref:null,isValid:function(){
return this.get("binding")?this.get("binding").get("valid"):true;
},_dbstartup:function(){
if(this._databound){
return;
}
this._unwatchArray(this._viewWatchHandles);
this._viewWatchHandles=[this.watch("ref",function(_5,_6,_7){
if(this._databound){
this._setupBinding();
}
}),this.watch("value",function(_8,_9,_a){
if(this._databound){
var _b=this.get("binding");
if(_b){
if(!((_a&&_9)&&(_9.valueOf()===_a.valueOf()))){
_b.set("value",_a);
}
}
}
})];
this._beingBound=true;
this._setupBinding();
delete this._beingBound;
this._databound=true;
},_setupBinding:function(_c){
if(!this.ref){
return;
}
var _d=this.ref,pw,pb,_e;
if(_d&&_1.isFunction(_d.toPlainObject)){
_e=_d;
}else{
if(/^\s*expr\s*:\s*/.test(_d)){
_d=_d.replace(/^\s*expr\s*:\s*/,"");
_e=_1.getObject(_d);
}else{
if(/^\s*rel\s*:\s*/.test(_d)){
_d=_d.replace(/^\s*rel\s*:\s*/,"");
_c=_c||this._getParentBindingFromDOM();
if(_c){
_e=_1.getObject(""+_d,false,_c);
}
}else{
if(/^\s*widget\s*:\s*/.test(_d)){
_d=_d.replace(/^\s*widget\s*:\s*/,"");
var _f=_d.split(".");
if(_f.length==1){
_e=_4.byId(_d).get("binding");
}else{
pb=_4.byId(_f.shift()).get("binding");
_e=_1.getObject(_f.join("."),false,pb);
}
}else{
_c=_c||this._getParentBindingFromDOM();
if(_c){
_e=_1.getObject(""+_d,false,_c);
}else{
try{
_e=_1.getObject(_d);
}
catch(err){
if(_d.indexOf("${")==-1){
throw new Error("dojox.mvc._DataBindingMixin: '"+this.domNode+"' widget with illegal ref expression: '"+_d+"'");
}
}
}
}
}
}
}
if(_e){
if(_1.isFunction(_e.toPlainObject)){
this.binding=_e;
this._updateBinding("binding",null,_e);
}else{
throw new Error("dojox.mvc._DataBindingMixin: '"+this.domNode+"' widget with illegal ref not evaluating to a dojo.Stateful node: '"+_d+"'");
}
}
},_isEqual:function(one,_10){
return one===_10||isNaN(one)&&typeof one==="number"&&isNaN(_10)&&typeof _10==="number";
},_updateBinding:function(_11,old,_12){
this._unwatchArray(this._modelWatchHandles);
var _13=this.get("binding");
if(_13&&_1.isFunction(_13.watch)){
var _14=this;
this._modelWatchHandles=[_13.watch("value",function(_15,old,_16){
if(_14._isEqual(old,_16)){
return;
}
if(_14._isEqual(_14.get("value"),_16)){
return;
}
_14.set("value",_16);
}),_13.watch("valid",function(_17,old,_18){
_14._updateProperty(_17,old,_18,true);
if(_18!==_14.get(_17)){
if(_14.validate&&_1.isFunction(_14.validate)){
_14.validate();
}
}
}),_13.watch("required",function(_19,old,_1a){
_14._updateProperty(_19,old,_1a,false,_19,_1a);
}),_13.watch("readOnly",function(_1b,old,_1c){
_14._updateProperty(_1b,old,_1c,false,_1b,_1c);
}),_13.watch("relevant",function(_1d,old,_1e){
_14._updateProperty(_1d,old,_1e,false,"disabled",!_1e);
})];
var val=_13.get("value");
if(val!=null){
this.set("value",val);
}
}
this._updateChildBindings();
},_updateProperty:function(_1f,old,_20,_21,_22,_23){
if(old===_20){
return;
}
if(_20===null&&_21!==undefined){
_20=_21;
}
if(_20!==this.get("binding").get(_1f)){
this.get("binding").set(_1f,_20);
}
if(_22){
this.set(_22,_23);
}
},_updateChildBindings:function(){
var _24=this.get("binding");
if(_24&&!this._beingBound){
_2.forEach(_4.findWidgets(this.domNode),function(_25){
if(_25._setupBinding){
_25._setupBinding(_24);
}
});
}
},_getParentBindingFromDOM:function(){
var pn=this.domNode.parentNode,pw,pb;
while(pn){
pw=_4.getEnclosingWidget(pn);
if(pw){
pb=pw.get("binding");
if(pb&&_1.isFunction(pb.toPlainObject)){
break;
}
}
pn=pw?pw.domNode.parentNode:null;
}
return pb;
},_unwatchArray:function(_26){
_2.forEach(_26,function(h){
h.unwatch();
});
}});
});
