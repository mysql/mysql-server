//>>built
define("dojox/app/controllers/Transition",["dojo/_base/lang","dojo/_base/declare","dojo/has","dojo/on","dojo/Deferred","dojo/when","dojox/css3/transit","../Controller"],function(_1,_2,_3,on,_4,_5,_6,_7){
return _2("dojox.app.controllers.Transition",_7,{proceeding:false,waitingQueue:[],constructor:function(_8,_9){
this.events={"transition":this.transition,"startTransition":this.onStartTransition};
this.inherited(arguments);
},transition:function(_a){
this.proceedTransition(_a);
},onStartTransition:function(_b){
if(_b.preventDefault){
_b.preventDefault();
}
_b.cancelBubble=true;
if(_b.stopPropagation){
_b.stopPropagation();
}
var _c=_b.detail.target;
var _d=/#(.+)/;
if(!_c&&_d.test(_b.detail.href)){
_c=_b.detail.href.match(_d)[1];
}
this.transition({"viewId":_c,opts:_1.mixin({},_b.detail)});
},proceedTransition:function(_e){
if(this.proceeding){
this.app.log("in app/controllers/Transition proceedTransition push event",_e);
this.waitingQueue.push(_e);
return;
}
this.proceeding=true;
this.app.log("in app/controllers/Transition proceedTransition calling trigger load",_e);
var _f=_e.params||{};
if(_e.opts&&_e.opts.params){
_f=_e.params||_e.opts.params;
}
this.app.trigger("load",{"viewId":_e.viewId,"params":_f,"callback":_1.hitch(this,function(){
var _10=this._doTransition(_e.viewId,_e.opts,_f,this.app);
_5(_10,_1.hitch(this,function(){
this.proceeding=false;
var _11=this.waitingQueue.shift();
if(_11){
this.proceedTransition(_11);
}
}));
})});
},_getDefaultTransition:function(_12){
var _13=_12;
var _14=_13.defaultTransition;
while(!_14&&_13.parent){
_13=_13.parent;
_14=_13.defaultTransition;
}
return _14;
},_doTransition:function(_15,_16,_17,_18){
this.app.log("in app/controllers/Transition._doTransition transitionTo=[",_15,"], parent.name=[",_18.name,"], opts=",_16);
if(!_18){
throw Error("view parent not found in transition.");
}
var _19,_1a,_1b,_1c,_17,_1d=_18.selectedChild;
if(_15){
_19=_15.split(",");
}else{
_19=_18.defaultView.split(",");
}
_1a=_19.shift();
_1b=_19.join(",");
_1c=_18.children[_18.id+"_"+_1a];
if(!_1c){
throw Error("child view must be loaded before transition.");
}
_1c.params=_17||_1c.params;
if(!_1b&&_1c.defaultView){
_1b=_1c.defaultView;
}
if(!_1d){
this.app.log("> in Transition._doTransition calling next.beforeActivate next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"],  !current path,");
_1c.beforeActivate();
this.app.log("> in Transition._doTransition calling next.afterActivate next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"],  !current path");
_1c.afterActivate();
this.app.log("  > in Transition._doTransition calling app.triggger select view next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], !current path");
this.app.trigger("select",{"parent":_18,"view":_1c});
return;
}
if(_1c!==_1d){
var _1e=_1d.selectedChild;
while(_1e){
this.app.log("< in Transition._doTransition calling subChild.beforeDeactivate subChild name=[",_1e.name,"], parent.name=[",_1e.parent.name,"], next!==current path");
_1e.beforeDeactivate();
_1e=_1e.selectedChild;
}
this.app.log("< in Transition._doTransition calling current.beforeDeactivate current name=[",_1d.name,"], parent.name=[",_1d.parent.name,"], next!==current path");
_1d.beforeDeactivate();
this.app.log("> in Transition._doTransition calling next.beforeActivate next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], next!==current path");
_1c.beforeActivate();
this.app.log("> in Transition._doTransition calling app.triggger select view next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], next!==current path");
this.app.trigger("select",{"parent":_18,"view":_1c});
var _1f=true;
if(!_3("ie")){
var _20=_1.mixin({},_16);
_20=_1.mixin({},_20,{reverse:(_20.reverse||_20.transitionDir===-1)?true:false,transition:_20.transition||this._getDefaultTransition(_18)||"none"});
_1f=_6(_1d.domNode,_1c.domNode,_20);
}
_5(_1f,_1.hitch(this,function(){
var _21=_1d.selectedChild;
while(_21){
this.app.log("  < in Transition._doTransition calling subChild.afterDeactivate subChild name=[",_21.name,"], parent.name=[",_21.parent.name,"], next!==current path");
_21.afterDeactivate();
_21=_21.selectedChild;
}
this.app.log("  < in Transition._doTransition calling current.afterDeactivate current name=[",_1d.name,"], parent.name=[",_1d.parent.name,"], next!==current path");
_1d.afterDeactivate();
this.app.log("  > in Transition._doTransition calling next.afterActivate next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], next!==current path");
_1c.afterActivate();
if(_1b){
this._doTransition(_1b,_16,_17,_1c);
}
}));
return _1f;
}else{
this.app.log("< in Transition._doTransition calling next.beforeDeactivate next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], next==current path");
_1c.beforeDeactivate();
this.app.log("  < in Transition._doTransition calling next.afterDeactivate next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], next==current path");
_1c.afterDeactivate();
this.app.log("> in Transition._doTransition calling next.beforeActivate next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], next==current path");
_1c.beforeActivate();
this.app.log("  > in Transition._doTransition calling next.afterActivate next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], next==current path");
_1c.afterActivate();
this.app.log("> in Transition._doTransition calling app.triggger select view next name=[",_1c.name,"], parent.name=[",_1c.parent.name,"], next==current path");
this.app.trigger("select",{"parent":_18,"view":_1c});
}
if(_1b){
return this._doTransition(_1b,_16,_17,_1c);
}
}});
});
