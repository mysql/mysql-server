//>>built
define("dojox/form/uploader/_IFrame",["dojo/query","dojo/dom-construct","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/dom-form","dojo/request/iframe"],function(_1,_2,_3,_4,_5,_6,_7){
return _3("dojox.form.uploader._IFrame",[],{postMixInProperties:function(){
this.inherited(arguments);
if(this.uploadType==="iframe"){
this.uploadType="iframe";
this.upload=this.uploadIFrame;
}
},uploadIFrame:function(_8){
var _9={},_a,_b=this.getForm(),_c=this.getUrl(),_d=this;
_8=_8||{};
_8.uploadType=this.uploadType;
_a=_2.place("<form enctype=\"multipart/form-data\" method=\"post\"></form>",this.domNode);
_5.forEach(this._inputs,function(n,i){
if(n.value!==""){
_a.appendChild(n);
_9[n.name]=n.value;
}
},this);
if(_8){
for(nm in _8){
if(_9[nm]===undefined){
_2.create("input",{name:nm,value:_8[nm],type:"hidden"},_a);
}
}
}
_7.post(_c,{form:_a,handleAs:"json",content:_8}).then(function(_e){
_2.destroy(_a);
if(_e["ERROR"]||_e["error"]){
_d.onError(_e);
}else{
_d.onComplete(_e);
}
},function(_f){
console.error("error parsing server result",_f);
_2.destroy(_a);
_d.onError(_f);
});
}});
});
