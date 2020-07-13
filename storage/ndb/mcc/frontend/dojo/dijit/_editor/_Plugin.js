//>>built
define("dijit/_editor/_Plugin",["dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","../Destroyable","../form/Button"],function(_1,_2,_3,_4,_5){
var _6=_2("dijit._editor._Plugin",_4,{constructor:function(_7){
this.params=_7||{};
_3.mixin(this,this.params);
this._attrPairNames={};
},editor:null,iconClassPrefix:"dijitEditorIcon",button:null,command:"",useDefaultCommand:true,buttonClass:_5,disabled:false,getLabel:function(_8){
return this.editor.commands[_8];
},_initButton:function(){
if(this.command.length){
var _9=this.getLabel(this.command),_a=this.editor,_b=this.iconClassPrefix+" "+this.iconClassPrefix+this.command.charAt(0).toUpperCase()+this.command.substr(1);
if(!this.button){
var _c=_3.mixin({label:_9,ownerDocument:_a.ownerDocument,dir:_a.dir,lang:_a.lang,showLabel:false,iconClass:_b,dropDown:this.dropDown,tabIndex:"-1"},this.params||{});
delete _c.name;
this.button=new this.buttonClass(_c);
}
}
if(this.get("disabled")&&this.button){
this.button.set("disabled",this.get("disabled"));
}
},destroy:function(){
if(this.dropDown){
this.dropDown.destroyRecursive();
}
this.inherited(arguments);
},connect:function(o,f,tf){
this.own(_1.connect(o,f,this,tf));
},updateState:function(){
var e=this.editor,c=this.command,_d,_e;
if(!e||!e.isLoaded||!c.length){
return;
}
var _f=this.get("disabled");
if(this.button){
try{
var _10=e._implCommand(c);
_e=!_f&&(this[_10]?this[_10](c):e.queryCommandEnabled(c));
if(this.enabled!==_e){
this.enabled=_e;
this.button.set("disabled",!_e);
}
if(_e){
if(typeof this.button.checked=="boolean"){
_d=e.queryCommandState(c);
if(this.checked!==_d){
this.checked=_d;
this.button.set("checked",e.queryCommandState(c));
}
}
}
}
catch(e){
}
}
},setEditor:function(_11){
this.editor=_11;
this._initButton();
if(this.button&&this.useDefaultCommand){
if(this.editor.queryCommandAvailable(this.command)){
this.own(this.button.on("click",_3.hitch(this.editor,"execCommand",this.command,this.commandArg)));
}else{
this.button.domNode.style.display="none";
}
}
this.own(this.editor.on("NormalizedDisplayChanged",_3.hitch(this,"updateState")));
},setToolbar:function(_12){
if(this.button){
_12.addChild(this.button);
}
},set:function(_13,_14){
if(typeof _13==="object"){
for(var x in _13){
this.set(x,_13[x]);
}
return this;
}
var _15=this._getAttrNames(_13);
if(this[_15.s]){
var _16=this[_15.s].apply(this,Array.prototype.slice.call(arguments,1));
}else{
this._set(_13,_14);
}
return _16||this;
},get:function(_17){
var _18=this._getAttrNames(_17);
return this[_18.g]?this[_18.g]():this[_17];
},_setDisabledAttr:function(_19){
this._set("disabled",_19);
this.updateState();
},_getAttrNames:function(_1a){
var apn=this._attrPairNames;
if(apn[_1a]){
return apn[_1a];
}
var uc=_1a.charAt(0).toUpperCase()+_1a.substr(1);
return (apn[_1a]={s:"_set"+uc+"Attr",g:"_get"+uc+"Attr"});
},_set:function(_1b,_1c){
this[_1b]=_1c;
}});
_6.registry={};
return _6;
});
