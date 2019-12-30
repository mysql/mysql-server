//>>built
define("dojox/widget/_Invalidating",["dojo/_base/declare","dojo/_base/lang","dojo/Stateful"],function(_1,_2,_3){
return _1("dojox.widget._Invalidating",_3,{invalidatingProperties:null,invalidRendering:false,postscript:function(_4){
this.inherited(arguments);
if(this.invalidatingProperties){
var _5=this.invalidatingProperties;
for(var i=0;i<_5.length;i++){
this.watch(_5[i],_2.hitch(this,this.invalidateRendering));
if(_4&&_5[i] in _4){
this.invalidateRendering();
}
}
}
},addInvalidatingProperties:function(_6){
this.invalidatingProperties=this.invalidatingProperties?this.invalidatingProperties.concat(_6):_6;
},invalidateRendering:function(){
if(!this.invalidRendering){
this.invalidRendering=true;
setTimeout(_2.hitch(this,this.validateRendering),0);
}
},validateRendering:function(){
if(this.invalidRendering){
this.refreshRendering();
this.invalidRendering=false;
}
},refreshRendering:function(){
}});
});
