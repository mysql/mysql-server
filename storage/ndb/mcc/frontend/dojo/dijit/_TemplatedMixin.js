//>>built
define("dijit/_TemplatedMixin",["dojo/_base/lang","dojo/touch","./_WidgetBase","dojo/string","dojo/cache","dojo/_base/array","dojo/_base/declare","dojo/dom-construct","dojo/_base/sniff","dojo/_base/unload","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_7("dijit._TemplatedMixin",null,{templateString:null,templatePath:null,_skipNodeCache:false,_earlyTemplatedStartup:false,constructor:function(){
this._attachPoints=[];
this._attachEvents=[];
},_stringRepl:function(_d){
var _e=this.declaredClass,_f=this;
return _4.substitute(_d,this,function(_10,key){
if(key.charAt(0)=="!"){
_10=_1.getObject(key.substr(1),false,_f);
}
if(typeof _10=="undefined"){
throw new Error(_e+" template:"+key);
}
if(_10==null){
return "";
}
return key.charAt(0)=="!"?_10:_10.toString().replace(/"/g,"&quot;");
},this);
},buildRendering:function(){
if(!this.templateString){
this.templateString=_5(this.templatePath,{sanitize:true});
}
var _11=_c.getCachedTemplate(this.templateString,this._skipNodeCache);
var _12;
if(_1.isString(_11)){
_12=_8.toDom(this._stringRepl(_11));
if(_12.nodeType!=1){
throw new Error("Invalid template: "+_11);
}
}else{
_12=_11.cloneNode(true);
}
this.domNode=_12;
this.inherited(arguments);
this._attachTemplateNodes(_12,function(n,p){
return n.getAttribute(p);
});
this._beforeFillContent();
this._fillContent(this.srcNodeRef);
},_beforeFillContent:function(){
},_fillContent:function(_13){
var _14=this.containerNode;
if(_13&&_14){
while(_13.hasChildNodes()){
_14.appendChild(_13.firstChild);
}
}
},_attachTemplateNodes:function(_15,_16){
var _17=_1.isArray(_15)?_15:(_15.all||_15.getElementsByTagName("*"));
var x=_1.isArray(_15)?0:-1;
for(;x<_17.length;x++){
var _18=(x==-1)?_15:_17[x];
if(this.widgetsInTemplate&&(_16(_18,"dojoType")||_16(_18,"data-dojo-type"))){
continue;
}
var _19=_16(_18,"dojoAttachPoint")||_16(_18,"data-dojo-attach-point");
if(_19){
var _1a,_1b=_19.split(/\s*,\s*/);
while((_1a=_1b.shift())){
if(_1.isArray(this[_1a])){
this[_1a].push(_18);
}else{
this[_1a]=_18;
}
this._attachPoints.push(_1a);
}
}
var _1c=_16(_18,"dojoAttachEvent")||_16(_18,"data-dojo-attach-event");
if(_1c){
var _1d,_1e=_1c.split(/\s*,\s*/);
var _1f=_1.trim;
while((_1d=_1e.shift())){
if(_1d){
var _20=null;
if(_1d.indexOf(":")!=-1){
var _21=_1d.split(":");
_1d=_1f(_21[0]);
_20=_1f(_21[1]);
}else{
_1d=_1f(_1d);
}
if(!_20){
_20=_1d;
}
this._attachEvents.push(this.connect(_18,_2[_1d]||_1d,_20));
}
}
}
}
},destroyRendering:function(){
_6.forEach(this._attachPoints,function(_22){
delete this[_22];
},this);
this._attachPoints=[];
_6.forEach(this._attachEvents,this.disconnect,this);
this._attachEvents=[];
this.inherited(arguments);
}});
_c._templateCache={};
_c.getCachedTemplate=function(_23,_24){
var _25=_c._templateCache;
var key=_23;
var _26=_25[key];
if(_26){
try{
if(!_26.ownerDocument||_26.ownerDocument==_b.doc){
return _26;
}
}
catch(e){
}
_8.destroy(_26);
}
_23=_4.trim(_23);
if(_24||_23.match(/\$\{([^\}]+)\}/g)){
return (_25[key]=_23);
}else{
var _27=_8.toDom(_23);
if(_27.nodeType!=1){
throw new Error("Invalid template: "+_23);
}
return (_25[key]=_27);
}
};
if(_9("ie")){
_a.addOnWindowUnload(function(){
var _28=_c._templateCache;
for(var key in _28){
var _29=_28[key];
if(typeof _29=="object"){
_8.destroy(_29);
}
delete _28[key];
}
});
}
_1.extend(_3,{dojoAttachEvent:"",dojoAttachPoint:""});
return _c;
});
