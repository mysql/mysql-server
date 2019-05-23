//>>built
define("dojox/widget/Rotator",["dojo/aspect","dojo/_base/declare","dojo/_base/Deferred","dojo/_base/lang","dojo/_base/array","dojo/_base/fx","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/topic","dojo/on","dojo/parser","dojo/query","dojo/fx/easing","dojo/NodeList-dom"],function(_1,_2,_3,_4,_5,fx,_6,_7,_8,_9,_a,_b,on,_c,_d){
var _e="dojox.widget.rotator.swap",_f=500,_10="display",_11="none",_12="zIndex";
var _13=_2("dojox.widget.Rotator",null,{transition:_e,transitionParams:"duration:"+_f,panes:null,constructor:function(_14,_15){
_4.mixin(this,_14);
var _16=this,t=_16.transition,tt=_16._transitions={},idm=_16._idMap={},tp=_16.transitionParams=eval("({ "+_16.transitionParams+" })"),_15=_16._domNode=_6.byId(_15),cb=_16._domNodeContentBox=_9.getContentBox(_15),p={left:0,top:0},_17=function(bt,dt){
console.warn(_16.declaredClass," - Unable to find transition \"",bt,"\", defaulting to \"",dt,"\".");
};
_16.id=_15.id||(new Date()).getTime();
if(_a.get(_15,"position")=="static"){
_a.set(_15,"position","relative");
}
tt[t]=_4.getObject(t);
if(!tt[t]){
_17(t,_e);
tt[_16.transition=_e]=_4.getObject(_e);
}
if(!tp.duration){
tp.duration=_f;
}
_5.forEach(_16.panes,function(p){
_8.create("div",p,_15);
});
var pp=_16.panes=[];
_d(">",_15).forEach(function(n,i){
var q={node:n,idx:i,params:_4.mixin({},tp,eval("({ "+(_7.get(n,"transitionParams")||"")+" })"))},r=q.trans=_7.get(n,"transition")||_16.transition;
_5.forEach(["id","title","duration","waitForEvent"],function(a){
q[a]=_7.get(n,a);
});
if(q.id){
idm[q.id]=i;
}
if(!tt[r]&&!(tt[r]=_4.getObject(r))){
_17(r,q.trans=_16.transition);
}
p.position="absolute";
p.display=_11;
if(_16.idx==null||_7.get(n,"selected")){
if(_16.idx!=null){
_a.set(pp[_16.idx].node,_10,_11);
}
_16.idx=i;
p.display="";
}
_a.set(n,p);
_d("> script[type^='dojo/method']",n).orphan().forEach(function(s){
var e=_7.get(s,"event");
if(e){
q[e]=_c._functionFromScript(s);
}
});
pp.push(q);
});
_16._controlSub=_b.subscribe(_16.id+"/rotator/control",_4.hitch(_16,this.control));
},destroy:function(){
_5.forEach([this._controlSub,this.wfe],function(wfe){
wfe.remove();
});
_8.destroy(this._domNode);
},next:function(){
return this.go(this.idx+1);
},prev:function(){
return this.go(this.idx-1);
},go:function(p){
var _18=this,i=_18.idx,pp=_18.panes,len=pp.length,idm=_18._idMap[p];
_18._resetWaitForEvent();
p=idm!=null?idm:(p||0);
p=p<len?(p<0?len-1:p):0;
if(p==i||_18.anim){
return null;
}
var _19=pp[i],_1a=pp[p];
_a.set(_19.node,_12,2);
_a.set(_1a.node,_12,1);
var _1b={current:_19,next:_1a,rotator:_18},_1c=_18.anim=_18._transitions[_1a.trans](_4.mixin({rotatorBox:_18._domNodeContentBox},_1b,_1a.params));
if(_1c){
var def=new _3(),ev=_1a.waitForEvent,h=_1.after(_1c,"onEnd",function(){
_a.set(_19.node,{display:_11,left:0,opacity:1,top:0,zIndex:0});
h.remove();
_18.anim=null;
_18.idx=p;
if(_19.onAfterOut){
_19.onAfterOut(_1b);
}
if(_1a.onAfterIn){
_1a.onAfterIn(_1b);
}
_18.onUpdate("onAfterTransition");
if(!ev){
_18._resetWaitForEvent();
def.callback();
}
},true);
_18.wfe=ev?_b.subscribe(ev,function(){
_18._resetWaitForEvent();
def.callback(true);
}):null;
_18.onUpdate("onBeforeTransition");
if(_19.onBeforeOut){
_19.onBeforeOut(_1b);
}
if(_1a.onBeforeIn){
_1a.onBeforeIn(_1b);
}
_1c.play();
return def;
}
},onUpdate:function(_1d,_1e){
_b.publish(this.id+"/rotator/update",_1d,this,_1e||{});
},_resetWaitForEvent:function(){
if(this.wfe){
this.wfe.remove();
delete this.wfe;
}
},control:function(_1f){
var _20=_4._toArray(arguments),_21=this;
_20.shift();
_21._resetWaitForEvent();
if(_21[_1f]){
var def=_21[_1f].apply(_21,_20);
if(def){
def.addCallback(function(){
_21.onUpdate(_1f);
});
}
_21.onManualChange(_1f);
}else{
console.warn(_21.declaredClass," - Unsupported action \"",_1f,"\".");
}
},resize:function(_22,_23){
var b=this._domNodeContentBox={w:_22,h:_23};
_9.setContentSize(this._domNode,b);
_5.forEach(this.panes,function(p){
_9.setContentSize(p.node,b);
});
},onManualChange:function(){
}});
_4.setObject(_e,function(_24){
return new fx.Animation({play:function(){
_a.set(_24.current.node,_10,_11);
_a.set(_24.next.node,_10,"");
this._fire("onEnd");
}});
});
return _13;
});
