//>>built
define("dojox/widget/Rotator",["dojo/aspect","dojo/_base/declare","dojo/_base/Deferred","dojo/_base/lang","dojo/_base/array","dojo/_base/fx","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/topic","dojo/on","dojo/parser","dojo/query","dojo/fx/easing","dojo/NodeList-dom"],function(_1,_2,_3,_4,_5,fx,_6,_7,_8,_9,_a,_b,on,_c,_d){
var _e="dojox.widget.rotator.swap",_f=500,_10="display",_11="none",_12="zIndex";
var _13=_2("dojox.widget.Rotator",null,{transition:_e,transitionParams:"duration:"+_f,panes:null,constructor:function(_14,_15){
_4.mixin(this,_14);
var _16=this,t=_16.transition,tt=_16._transitions={},idm=_16._idMap={},tp=_16.transitionParams=eval("({ "+_16.transitionParams+" })"),_15=_16._domNode=_6.byId(_15),cb=_16._domNodeContentBox=_9.getContentBox(_15);
_16.id=_15.id||(new Date()).getTime();
if(_a.get(_15,"position")=="static"){
_a.set(_15,"position","relative");
}
tt[t]=_4.getObject(t);
if(!tt[t]){
this._transitionWarn(t,_e);
tt[_16.transition=_e]=_4.getObject(_e);
}
if(!tp.duration){
tp.duration=_f;
}
_5.forEach(_16.panes,function(p){
_8.create("div",p,_15);
});
_16.panes=[];
_d("> *",_15).forEach(function(n,i){
_16._initializePane(n,i);
});
_16._controlSub=_b.subscribe(_16.id+"/rotator/control",_4.hitch(_16,this.control));
},insert:function(_17,_18){
var _19,_1a=this.panes,_1b;
if(_18==null){
_18=_1a.length;
}
if(_18<_1a.length){
_19=_1a[_18];
_8.place(_17,_19.node,"before");
}else{
_8.place(_17,this._domNode,"last");
}
this._initializePane(_17,_18);
},remove:function(_1c){
function _1d(idx){
var _1e=_1f.splice(idx,1)[0];
if(_1e){
if(_1e.id){
_20._idMap[_1e.id]=undefined;
}
_20._domNode.removeChild(_1e.node);
}
if(_20.idx>idx){
_20.idx--;
}
};
var _21,_20=this,_1f=this.panes;
if(typeof _1c==="number"){
_21=_1c;
}else{
for(var i=0;i<_1f.length;i++){
if(_1f[i].node===_1c){
_21=i;
break;
}
}
if(_21==null){
return;
}
}
if(_21===this.idx){
var def=this.go(this.idx-1);
if(def){
return def.then(function(){
_1d(_21);
});
}else{
_1d(_21);
}
}else{
_1d(_21);
}
},destroy:function(){
_5.forEach([this._controlSub,this.wfe],function(wfe){
wfe&&wfe.remove();
});
_8.destroy(this._domNode);
this.panes=[];
},next:function(){
return this.go(this.idx+1);
},prev:function(){
return this.go(this.idx-1);
},go:function(p){
var _22=this,i=_22.idx,pp=_22.panes,len=pp.length,idm=_22._idMap[p];
_22._resetWaitForEvent();
p=idm!=null?idm:(p||0);
p=p<len?(p<0?len-1:p):0;
if(p==i||_22.anim){
return null;
}
var _23=pp[i],_24=pp[p];
_a.set(_23.node,_12,2);
_a.set(_24.node,_12,1);
var _25={current:_23,next:_24,rotator:_22},_26=_22.anim=_22._transitions[_24.trans](_4.mixin({rotatorBox:_22._domNodeContentBox},_25,_24.params));
if(_26){
var def=new _3(),ev=_24.waitForEvent,h=_1.after(_26,"onEnd",function(){
_a.set(_23.node,{display:_11,left:0,opacity:1,top:0,zIndex:0});
h.remove();
_22.anim=null;
_22.idx=p;
if(_23.onAfterOut){
_23.onAfterOut(_25);
}
if(_24.onAfterIn){
_24.onAfterIn(_25);
}
_22.onUpdate("onAfterTransition");
if(!ev){
_22._resetWaitForEvent();
def.callback();
}
},true);
_22.wfe=ev?_b.subscribe(ev,function(){
_22._resetWaitForEvent();
def.callback(true);
}):null;
_22.onUpdate("onBeforeTransition");
if(_23.onBeforeOut){
_23.onBeforeOut(_25);
}
if(_24.onBeforeIn){
_24.onBeforeIn(_25);
}
_26.play();
return def;
}
},onUpdate:function(_27,_28){
_b.publish(this.id+"/rotator/update",_27,this,_28||{});
},_initializePane:function(_29,_2a){
var tp=this.transitionParams,q={node:_29,idx:_2a,params:_4.mixin({},tp,eval("({ "+(_7.get(_29,"transitionParams")||"")+" })"))},r=q.trans=_7.get(_29,"transition")||this.transition,tt=this._transitions,_2b=this.panes,p={left:0,top:0,position:"absolute",display:_11};
_5.forEach(["id","title","duration","waitForEvent"],function(a){
q[a]=_7.get(_29,a);
});
if(q.id){
this._idMap[q.id]=_2a;
}
if(!tt[r]&&!(tt[r]=_4.getObject(r))){
this._transitionWarn(r,q.trans=this.transition);
}
if(this.idx==null||_7.get(_29,"selected")){
if(this.idx!=null){
_a.set(_2b[this.idx].node,_10,_11);
}
this.idx=_2a;
p.display="";
}
_a.set(_29,p);
_d("> script[type^='dojo/method']",_29).orphan().forEach(function(s){
var e=_7.get(s,"event");
if(e){
q[e]=_c._functionFromScript(s);
}
});
_2b.splice(_2a,0,q);
},_resetWaitForEvent:function(){
if(this.wfe){
this.wfe.remove();
delete this.wfe;
}
},control:function(_2c){
var _2d=_4._toArray(arguments),_2e=this;
_2d.shift();
_2e._resetWaitForEvent();
if(_2e[_2c]){
var def=_2e[_2c].apply(_2e,_2d);
if(def){
def.addCallback(function(){
_2e.onUpdate(_2c);
});
}
_2e.onManualChange(_2c);
}else{
console.warn(_2e.declaredClass," - Unsupported action \"",_2c,"\".");
}
},resize:function(_2f,_30){
var b=this._domNodeContentBox={w:_2f,h:_30};
_9.setContentSize(this._domNode,b);
_5.forEach(this.panes,function(p){
_9.setContentSize(p.node,b);
});
},onManualChange:function(){
},_transitionWarn:function(bt,dt){
console.warn(this.declaredClass," - Unable to find transition \"",bt,"\", defaulting to \"",dt,"\".");
}});
_4.setObject(_e,function(_31){
return new fx.Animation({play:function(){
_a.set(_31.current.node,_10,_11);
_a.set(_31.next.node,_10,"");
this._fire("onEnd");
}});
});
return _13;
});
