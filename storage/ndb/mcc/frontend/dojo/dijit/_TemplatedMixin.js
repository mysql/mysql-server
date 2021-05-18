//>>built
define("dijit/_TemplatedMixin",["dojo/cache","dojo/_base/declare","dojo/dom-construct","dojo/_base/lang","dojo/on","dojo/sniff","dojo/string","./_AttachMixin"],function(_1,_2,_3,_4,on,_5,_6,_7){
var _8=_2("dijit._TemplatedMixin",_7,{templateString:null,templatePath:null,_skipNodeCache:false,searchContainerNode:true,_stringRepl:function(_9){
var _a=this.declaredClass,_b=this;
return _6.substitute(_9,this,function(_c,_d){
if(_d.charAt(0)=="!"){
_c=_4.getObject(_d.substr(1),false,_b);
}
if(typeof _c=="undefined"){
throw new Error(_a+" template:"+_d);
}
if(_c==null){
return "";
}
return _d.charAt(0)=="!"?_c:this._escapeValue(""+_c);
},this);
},_escapeValue:function(_e){
return _e.replace(/["'<>&]/g,function(_f){
return {"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;","'":"&#x27;"}[_f];
});
},buildRendering:function(){
if(!this._rendered){
if(!this.templateString){
this.templateString=_1(this.templatePath,{sanitize:true});
}
var _10=_8.getCachedTemplate(this.templateString,this._skipNodeCache,this.ownerDocument);
var _11;
if(_4.isString(_10)){
_11=_3.toDom(this._stringRepl(_10),this.ownerDocument);
if(_11.nodeType!=1){
throw new Error("Invalid template: "+_10);
}
}else{
_11=_10.cloneNode(true);
}
this.domNode=_11;
}
this.inherited(arguments);
if(!this._rendered){
this._fillContent(this.srcNodeRef);
}
this._rendered=true;
},_fillContent:function(_12){
var _13=this.containerNode;
if(_12&&_13){
while(_12.hasChildNodes()){
_13.appendChild(_12.firstChild);
}
}
}});
_8._templateCache={};
_8.getCachedTemplate=function(_14,_15,doc){
var _16=_8._templateCache;
var key=_14;
var _17=_16[key];
if(_17){
try{
if(!_17.ownerDocument||_17.ownerDocument==(doc||document)){
return _17;
}
}
catch(e){
}
_3.destroy(_17);
}
_14=_6.trim(_14);
if(_15||_14.match(/\$\{([^\}]+)\}/g)){
return (_16[key]=_14);
}else{
var _18=_3.toDom(_14,doc);
if(_18.nodeType!=1){
throw new Error("Invalid template: "+_14);
}
return (_16[key]=_18);
}
};
if(_5("ie")){
on(window,"unload",function(){
var _19=_8._templateCache;
for(var key in _19){
var _1a=_19[key];
if(typeof _1a=="object"){
_3.destroy(_1a);
}
delete _19[key];
}
});
}
return _8;
});
