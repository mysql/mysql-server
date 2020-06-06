//>>built
define("dojox/layout/ContentPane",["dojo/_base/lang","dojo/_base/xhr","dijit/layout/ContentPane","dojox/html/_base","dojo/_base/declare"],function(_1,_2,_3,_4,_5){
return _5("dojox.layout.ContentPane",_3,{adjustPaths:false,cleanContent:false,renderStyles:false,executeScripts:true,scriptHasHooks:false,ioMethod:_2.get,ioArgs:{},onExecError:function(e){
},_setContent:function(_6){
var _7=this._contentSetter;
if(!(_7&&_7 instanceof _4._ContentSetter)){
_7=this._contentSetter=new _4._ContentSetter({node:this.containerNode,_onError:_1.hitch(this,this._onError),onContentError:_1.hitch(this,function(e){
var _8=this.onContentError(e);
try{
this.containerNode.innerHTML=_8;
}
catch(e){
console.error("Fatal "+this.id+" could not change content due to "+e.message,e);
}
})});
}
this._contentSetterParams={adjustPaths:Boolean(this.adjustPaths&&(this.href||this.referencePath)),referencePath:this.href||this.referencePath,renderStyles:this.renderStyles,executeScripts:this.executeScripts,scriptHasHooks:this.scriptHasHooks,scriptHookReplacement:"dijit.byId('"+this.id+"')"};
return this.inherited("_setContent",arguments);
},destroy:function(){
var _9=this._contentSetter;
if(_9){
_9.tearDown();
}
this.inherited(arguments);
}});
});
