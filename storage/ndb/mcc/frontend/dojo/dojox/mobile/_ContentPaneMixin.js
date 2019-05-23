//>>built
define("dojox/mobile/_ContentPaneMixin",["dojo/_base/declare","dojo/_base/Deferred","dojo/_base/lang","dojo/_base/window","dojo/_base/xhr","./_ExecScriptMixin","./ProgressIndicator","./lazyLoadUtils"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.mobile._ContentPaneMixin",_6,{href:"",lazy:false,content:"",parseOnLoad:true,prog:true,executeScripts:true,constructor:function(){
if(this.prog){
this._p=_7.getInstance();
}
},loadHandler:function(_9){
this.set("content",_9);
},errorHandler:function(_a){
if(this._p){
this._p.stop();
}
},load:function(){
this.lazy=false;
this.set("href",this.href);
},onLoad:function(){
return true;
},_setHrefAttr:function(_b){
if(this.lazy||!_b||_b===this._loaded){
this.lazy=false;
return null;
}
var p=this._p;
if(p){
_4.body().appendChild(p.domNode);
p.start();
}
this._set("href",_b);
this._loaded=_b;
return _5.get({url:_b,handleAs:"text",load:_3.hitch(this,"loadHandler"),error:_3.hitch(this,"errorHandler")});
},_setContentAttr:function(_c){
this.destroyDescendants();
if(typeof _c==="object"){
this.containerNode.appendChild(_c);
}else{
if(this.executeScripts){
_c=this.execScript(_c);
}
this.containerNode.innerHTML=_c;
}
if(this.parseOnLoad){
var _d=this;
return _2.when(_8.instantiateLazyWidgets(_d.containerNode),function(){
if(_d._p){
_d._p.stop();
}
return _d.onLoad();
});
}
if(this._p){
this._p.stop();
}
return this.onLoad();
}});
});
