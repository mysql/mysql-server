//>>built
define("dojox/widget/rotator/ThumbnailController",["dojo/_base/declare","dojo/_base/connect","dojo/_base/lang","dojo/_base/event","dojo/aspect","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/query"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a="dojoxRotatorThumb",_b=_a+"Selected";
return _1("dojox.widget.rotator.ThumbnailController",null,{rotator:null,constructor:function(_c,_d){
_3.mixin(this,_c);
this._domNode=_d;
var r=this.rotator;
if(r){
while(_d.firstChild){
_d.removeChild(_d.firstChild);
}
for(var i=0;i<r.panes.length;i++){
var n=r.panes[i].node,s=_6.get(n,"thumbsrc")||_6.get(n,"src"),t=_6.get(n,"alt")||"";
if(/img/i.test(n.tagName)){
(function(j){
_8.create("a",{classname:_a+" "+_a+j+" "+(j==r.idx?_b:""),href:s,onclick:function(e){
_4.stop(e);
if(r){
r.control.apply(r,["go",j]);
}
},title:t,innerHTML:"<img src=\""+s+"\" alt=\""+t+"\"/>"},_d);
})(i);
}
}
_5.after(r,"onUpdate",_3.hitch(this,"_onUpdate"),true);
}
},destroy:function(){
_8.destroy(this._domNode);
},_onUpdate:function(_e){
var r=this.rotator;
if(_e=="onAfterTransition"){
var n=_9("."+_a,this._domNode).removeClass(_b);
if(r.idx<n.length){
_7.add(n[r.idx],_b);
}
}
}});
});
