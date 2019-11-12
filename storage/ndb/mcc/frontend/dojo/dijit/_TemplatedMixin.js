//>>built
define("dijit/_TemplatedMixin",["dojo/_base/lang","dojo/touch","./_WidgetBase","dojo/string","dojo/cache","dojo/_base/array","dojo/_base/declare","dojo/dom-construct","dojo/sniff","dojo/_base/unload"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_7("dijit._TemplatedMixin",null,{templateString:null,templatePath:null,_skipNodeCache:false,_earlyTemplatedStartup:false,constructor:function(){
this._attachPoints=[];
this._attachEvents=[];
},_stringRepl:function(_c){
var _d=this.declaredClass,_e=this;
return _4.substitute(_c,this,function(_f,key){
if(key.charAt(0)=="!"){
_f=_1.getObject(key.substr(1),false,_e);
}
if(typeof _f=="undefined"){
throw new Error(_d+" template:"+key);
}
if(_f==null){
return "";
}
return key.charAt(0)=="!"?_f:_f.toString().replace(/"/g,"&quot;");
},this);
},buildRendering:function(){
if(!this.templateString){
this.templateString=_5(this.templatePath,{sanitize:true});
}
var _10=_b.getCachedTemplate(this.templateString,this._skipNodeCache,this.ownerDocument);
var _11;
if(_1.isString(_10)){
_11=_8.toDom(this._stringRepl(_10),this.ownerDocument);
if(_11.nodeType!=1){
throw new Error("Invalid template: "+_10);
}
}else{
_11=_10.cloneNode(true);
}
this.domNode=_11;
this.inherited(arguments);
this._attachTemplateNodes(_11,function(n,p){
return n.getAttribute(p);
});
this._beforeFillContent();
this._fillContent(this.srcNodeRef);
},_beforeFillContent:function(){
},_fillContent:function(_12){
var _13=this.containerNode;
if(_12&&_13){
while(_12.hasChildNodes()){
_13.appendChild(_12.firstChild);
}
}
},_attachTemplateNodes:function(_14,_15){
var _16=_1.isArray(_14)?_14:(_14.all||_14.getElementsByTagName("*"));
var x=_1.isArray(_14)?0:-1;
for(;x<0||_16[x];x++){
var _17=(x==-1)?_14:_16[x];
if(this.widgetsInTemplate&&(_15(_17,"dojoType")||_15(_17,"data-dojo-type"))){
continue;
}
var _18=_15(_17,"dojoAttachPoint")||_15(_17,"data-dojo-attach-point");
if(_18){
var _19,_1a=_18.split(/\s*,\s*/);
while((_19=_1a.shift())){
if(_1.isArray(this[_19])){
this[_19].push(_17);
}else{
this[_19]=_17;
}
this._attachPoints.push(_19);
}
}
var _1b=_15(_17,"dojoAttachEvent")||_15(_17,"data-dojo-attach-event");
if(_1b){
var _1c,_1d=_1b.split(/\s*,\s*/);
var _1e=_1.trim;
while((_1c=_1d.shift())){
if(_1c){
var _1f=null;
if(_1c.indexOf(":")!=-1){
var _20=_1c.split(":");
_1c=_1e(_20[0]);
_1f=_1e(_20[1]);
}else{
_1c=_1e(_1c);
}
if(!_1f){
_1f=_1c;
}
this._attachEvents.push(this.connect(_17,_2[_1c]||_1c,_1f));
}
}
}
}
},destroyRendering:function(){
_6.forEach(this._attachPoints,function(_21){
delete this[_21];
},this);
this._attachPoints=[];
_6.forEach(this._attachEvents,this.disconnect,this);
this._attachEvents=[];
this.inherited(arguments);
}});
_b._templateCache={};
_b.getCachedTemplate=function(_22,_23,doc){
var _24=_b._templateCache;
var key=_22;
var _25=_24[key];
if(_25){
try{
if(!_25.ownerDocument||_25.ownerDocument==(doc||document)){
return _25;
}
}
catch(e){
}
_8.destroy(_25);
}
_22=_4.trim(_22);
if(_23||_22.match(/\$\{([^\}]+)\}/g)){
return (_24[key]=_22);
}else{
var _26=_8.toDom(_22,doc);
if(_26.nodeType!=1){
throw new Error("Invalid template: "+_22);
}
return (_24[key]=_26);
}
};
if(_9("ie")){
_a.addOnWindowUnload(function(){
var _27=_b._templateCache;
for(var key in _27){
var _28=_27[key];
if(typeof _28=="object"){
_8.destroy(_28);
}
delete _27[key];
}
});
}
_1.extend(_3,{dojoAttachEvent:"",dojoAttachPoint:""});
return _b;
});
