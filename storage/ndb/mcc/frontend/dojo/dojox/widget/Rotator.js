//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/parser"],function(_1,_2,_3){
_2.provide("dojox.widget.Rotator");
_2.require("dojo.parser");
(function(d){
var _4="dojox.widget.rotator.swap",_5=500,_6="display",_7="none",_8="zIndex";
d.declare("dojox.widget.Rotator",null,{transition:_4,transitionParams:"duration:"+_5,panes:null,constructor:function(_9,_a){
d.mixin(this,_9);
var _b=this,t=_b.transition,tt=_b._transitions={},_c=_b._idMap={},tp=_b.transitionParams=eval("({ "+_b.transitionParams+" })"),_a=_b._domNode=_2.byId(_a),cb=_b._domNodeContentBox=d.contentBox(_a),p={left:0,top:0},_d=function(bt,dt){
console.warn(_b.declaredClass," - Unable to find transition \"",bt,"\", defaulting to \"",dt,"\".");
};
_b.id=_a.id||(new Date()).getTime();
if(d.style(_a,"position")=="static"){
d.style(_a,"position","relative");
}
tt[t]=d.getObject(t);
if(!tt[t]){
_d(t,_4);
tt[_b.transition=_4]=d.getObject(_4);
}
if(!tp.duration){
tp.duration=_5;
}
d.forEach(_b.panes,function(p){
d.create("div",p,_a);
});
var pp=_b.panes=[];
d.query(">",_a).forEach(function(n,i){
var q={node:n,idx:i,params:d.mixin({},tp,eval("({ "+(d.attr(n,"transitionParams")||"")+" })"))},r=q.trans=d.attr(n,"transition")||_b.transition;
d.forEach(["id","title","duration","waitForEvent"],function(a){
q[a]=d.attr(n,a);
});
if(q.id){
_c[q.id]=i;
}
if(!tt[r]&&!(tt[r]=d.getObject(r))){
_d(r,q.trans=_b.transition);
}
p.position="absolute";
p.display=_7;
if(_b.idx==null||d.attr(n,"selected")){
if(_b.idx!=null){
d.style(pp[_b.idx].node,_6,_7);
}
_b.idx=i;
p.display="";
}
d.style(n,p);
d.query("> script[type^='dojo/method']",n).orphan().forEach(function(s){
var e=d.attr(s,"event");
if(e){
q[e]=d.parser._functionFromScript(s);
}
});
pp.push(q);
});
_b._controlSub=d.subscribe(_b.id+"/rotator/control",_b,"control");
},destroy:function(){
d.forEach([this._controlSub,this.wfe],d.unsubscribe);
d.destroy(this._domNode);
},next:function(){
return this.go(this.idx+1);
},prev:function(){
return this.go(this.idx-1);
},go:function(p){
var _e=this,i=_e.idx,pp=_e.panes,_f=pp.length,idm=_e._idMap[p];
_e._resetWaitForEvent();
p=idm!=null?idm:(p||0);
p=p<_f?(p<0?_f-1:p):0;
if(p==i||_e.anim){
return null;
}
var _10=pp[i],_11=pp[p];
d.style(_10.node,_8,2);
d.style(_11.node,_8,1);
var _12={current:_10,next:_11,rotator:_e},_13=_e.anim=_e._transitions[_11.trans](d.mixin({rotatorBox:_e._domNodeContentBox},_12,_11.params));
if(_13){
var def=new d.Deferred(),ev=_11.waitForEvent,h=d.connect(_13,"onEnd",function(){
d.style(_10.node,{display:_7,left:0,opacity:1,top:0,zIndex:0});
d.disconnect(h);
_e.anim=null;
_e.idx=p;
if(_10.onAfterOut){
_10.onAfterOut(_12);
}
if(_11.onAfterIn){
_11.onAfterIn(_12);
}
_e.onUpdate("onAfterTransition");
if(!ev){
_e._resetWaitForEvent();
def.callback();
}
});
_e.wfe=ev?d.subscribe(ev,function(){
_e._resetWaitForEvent();
def.callback(true);
}):null;
_e.onUpdate("onBeforeTransition");
if(_10.onBeforeOut){
_10.onBeforeOut(_12);
}
if(_11.onBeforeIn){
_11.onBeforeIn(_12);
}
_13.play();
return def;
}
},onUpdate:function(_14,_15){
d.publish(this.id+"/rotator/update",[_14,this,_15||{}]);
},_resetWaitForEvent:function(){
if(this.wfe){
d.unsubscribe(this.wfe);
this.wfe=null;
}
},control:function(_16){
var _17=d._toArray(arguments),_18=this;
_17.shift();
_18._resetWaitForEvent();
if(_18[_16]){
var def=_18[_16].apply(_18,_17);
if(def){
def.addCallback(function(){
_18.onUpdate(_16);
});
}
_18.onManualChange(_16);
}else{
console.warn(_18.declaredClass," - Unsupported action \"",_16,"\".");
}
},resize:function(_19,_1a){
var b=this._domNodeContentBox={w:_19,h:_1a};
d.contentBox(this._domNode,b);
d.forEach(this.panes,function(p){
d.contentBox(p.node,b);
});
},onManualChange:function(){
}});
d.setObject(_4,function(_1b){
return new d._Animation({play:function(){
d.style(_1b.current.node,_6,_7);
d.style(_1b.next.node,_6,"");
this._fire("onEnd");
}});
});
})(_2);
});
