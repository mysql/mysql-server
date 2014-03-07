//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.widget.rotator.ThumbnailController");
(function(d){
var _4="dojoxRotatorThumb",_5=_4+"Selected";
d.declare("dojox.widget.rotator.ThumbnailController",null,{rotator:null,constructor:function(_6,_7){
d.mixin(this,_6);
this._domNode=_7;
var r=this.rotator;
if(r){
while(_7.firstChild){
_7.removeChild(_7.firstChild);
}
for(var i=0;i<r.panes.length;i++){
var n=r.panes[i].node,s=d.attr(n,"thumbsrc")||d.attr(n,"src"),t=d.attr(n,"alt")||"";
if(/img/i.test(n.tagName)){
(function(j){
d.create("a",{classname:_4+" "+_4+j+" "+(j==r.idx?_5:""),href:s,onclick:function(e){
d.stopEvent(e);
if(r){
r.control.apply(r,["go",j]);
}
},title:t,innerHTML:"<img src=\""+s+"\" alt=\""+t+"\"/>"},_7);
})(i);
}
}
this._con=d.connect(r,"onUpdate",this,"_onUpdate");
}
},destroy:function(){
d.disconnect(this._con);
d.destroy(this._domNode);
},_onUpdate:function(_8){
var r=this.rotator;
if(_8=="onAfterTransition"){
var n=d.query("."+_4,this._domNode).removeClass(_5);
if(r.idx<n.length){
d.addClass(n[r.idx],_5);
}
}
}});
})(_2);
});
