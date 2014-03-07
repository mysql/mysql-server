//>>built
define("dojox/form/uploader/plugins/IFrame",["dojo/dom-construct","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/io/iframe","dojox/form/uploader/plugins/HTML5"],function(_1,_2,_3,_4,_5,_6){
var _7=_2("dojox.form.uploader.plugins.IFrame",[],{force:"",postMixInProperties:function(){
this.inherited(arguments);
if(!this.supports("multiple")||this.force=="iframe"){
this.uploadType="iframe";
this.upload=this.uploadIFrame;
}
},uploadIFrame:function(_8){
var _9,_a=false;
if(!this.getForm()){
_9=_1.place("<form enctype=\"multipart/form-data\" method=\"post\"></form>",this.domNode);
_4.forEach(this._inputs,function(n,i){
if(n.value){
_9.appendChild(n);
}
},this);
_a=true;
}else{
_9=this.form;
}
var _b=this.getUrl();
var _c=_5.send({url:_b,form:_9,handleAs:"json",content:_8,error:_3.hitch(this,function(_d){
if(_a){
_1.destroy(_9);
}
this.onError(_d);
}),load:_3.hitch(this,function(_e,_f,_10){
if(_a){
_1.destroy(_9);
}
if(_e["ERROR"]||_e["error"]){
this.onError(_e);
}else{
this.onComplete(_e);
}
})});
}});
dojox.form.addUploaderPlugin(_7);
return _7;
});
