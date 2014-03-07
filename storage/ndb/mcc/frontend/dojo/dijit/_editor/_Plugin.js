//>>built
define("dijit/_editor/_Plugin",["dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","../form/Button"],function(_1,_2,_3,_4){
var _5=_2("dijit._editor._Plugin",null,{constructor:function(_6){
this.params=_6||{};
_3.mixin(this,this.params);
this._connects=[];
this._attrPairNames={};
},editor:null,iconClassPrefix:"dijitEditorIcon",button:null,command:"",useDefaultCommand:true,buttonClass:_4,disabled:false,getLabel:function(_7){
return this.editor.commands[_7];
},_initButton:function(){
if(this.command.length){
var _8=this.getLabel(this.command),_9=this.editor,_a=this.iconClassPrefix+" "+this.iconClassPrefix+this.command.charAt(0).toUpperCase()+this.command.substr(1);
if(!this.button){
var _b=_3.mixin({label:_8,dir:_9.dir,lang:_9.lang,showLabel:false,iconClass:_a,dropDown:this.dropDown,tabIndex:"-1"},this.params||{});
this.button=new this.buttonClass(_b);
}
}
if(this.get("disabled")&&this.button){
this.button.set("disabled",this.get("disabled"));
}
},destroy:function(){
var h;
while(h=this._connects.pop()){
h.remove();
}
if(this.dropDown){
this.dropDown.destroyRecursive();
}
},connect:function(o,f,tf){
this._connects.push(_1.connect(o,f,this,tf));
},updateState:function(){
var e=this.editor,c=this.command,_c,_d;
if(!e||!e.isLoaded||!c.length){
return;
}
var _e=this.get("disabled");
if(this.button){
try{
_d=!_e&&e.queryCommandEnabled(c);
if(this.enabled!==_d){
this.enabled=_d;
this.button.set("disabled",!_d);
}
if(typeof this.button.checked=="boolean"){
_c=e.queryCommandState(c);
if(this.checked!==_c){
this.checked=_c;
this.button.set("checked",e.queryCommandState(c));
}
}
}
catch(e){
}
}
},setEditor:function(_f){
this.editor=_f;
this._initButton();
if(this.button&&this.useDefaultCommand){
if(this.editor.queryCommandAvailable(this.command)){
this.connect(this.button,"onClick",_3.hitch(this.editor,"execCommand",this.command,this.commandArg));
}else{
this.button.domNode.style.display="none";
}
}
this.connect(this.editor,"onNormalizedDisplayChanged","updateState");
},setToolbar:function(_10){
if(this.button){
_10.addChild(this.button);
}
},set:function(_11,_12){
if(typeof _11==="object"){
for(var x in _11){
this.set(x,_11[x]);
}
return this;
}
var _13=this._getAttrNames(_11);
if(this[_13.s]){
var _14=this[_13.s].apply(this,Array.prototype.slice.call(arguments,1));
}else{
this._set(_11,_12);
}
return _14||this;
},get:function(_15){
var _16=this._getAttrNames(_15);
return this[_16.g]?this[_16.g]():this[_15];
},_setDisabledAttr:function(_17){
this.disabled=_17;
this.updateState();
},_getAttrNames:function(_18){
var apn=this._attrPairNames;
if(apn[_18]){
return apn[_18];
}
var uc=_18.charAt(0).toUpperCase()+_18.substr(1);
return (apn[_18]={s:"_set"+uc+"Attr",g:"_get"+uc+"Attr"});
},_set:function(_19,_1a){
this[_19]=_1a;
}});
_5.registry={};
return _5;
});
