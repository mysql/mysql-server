//>>built
define("dojox/app/controllers/Transition",["require","dojo/_base/lang","dojo/_base/declare","dojo/has","dojo/on","dojo/Deferred","dojo/when","dojo/dom-style","../Controller","../utils/constraints"],function(_1,_2,_3,_4,on,_5,_6,_7,_8,_9){
var _a;
var _b="app/controllers/Transition";
var _c="logTransitions:";
return _3("dojox.app.controllers.Transition",_8,{proceeding:false,waitingQueue:[],constructor:function(_d,_e){
this.events={"app-transition":this.transition,"app-domNode":this.onDomNodeChange};
_1([this.app.transit||"dojox/css3/transit"],function(t){
_a=t;
});
if(this.app.domNode){
this.onDomNodeChange({oldNode:null,newNode:this.app.domNode});
}
},transition:function(_f){
var F=_b+":transition";
this.app.log(_c,F," ");
this.app.log(_c,F,"New Transition event.viewId=["+_f.viewId+"]");
this.app.log(F,"event.viewId=["+_f.viewId+"]","event.opts=",_f.opts);
var _10=_f.viewId||"";
this.proceedingSaved=this.proceeding;
var _11=_10.split("+");
var _12=_10.split("-");
var _13,_14;
if(_11.length>0||_12.length>0){
while(_11.length>1){
_13=_11.shift();
_14=_2.clone(_f);
if(_13.indexOf("-")>=0){
var _15=_13.split("-");
if(_15.length>0){
_13=_15.shift();
if(_13){
_14._removeView=false;
_14.viewId=_13;
this.proceeding=true;
this.proceedTransition(_14);
_14=_2.clone(_f);
}
_13=_15.shift();
if(_13){
_14._removeView=true;
_14.viewId=_13;
this.proceeding=true;
this.proceedTransition(_14);
}
}
}else{
_14._removeView=false;
_14.viewId=_13;
this.proceeding=true;
this.proceedTransition(_14);
}
}
_13=_11.shift();
var _15=_13.split("-");
if(_15.length>0){
_13=_15.shift();
}
if(_13.length>0){
this.proceeding=this.proceedingSaved;
_f.viewId=_13;
_f._doResize=true;
_f._removeView=false;
this.proceedTransition(_f);
}
if(_15.length>0){
while(_15.length>0){
var _16=_15.shift();
_14=_2.clone(_f);
_14.viewId=_16;
_14._removeView=true;
_14._doResize=true;
this.proceedTransition(_14);
}
}
}else{
_f._doResize=true;
_f._removeView=false;
this.proceedTransition(_f);
}
},onDomNodeChange:function(evt){
if(evt.oldNode!=null){
this.unbind(evt.oldNode,"startTransition");
}
this.bind(evt.newNode,"startTransition",_2.hitch(this,this.onStartTransition));
},onStartTransition:function(evt){
if(evt.preventDefault){
evt.preventDefault();
}
evt.cancelBubble=true;
if(evt.stopPropagation){
evt.stopPropagation();
}
var _17=evt.detail.target;
var _18=/#(.+)/;
if(!_17&&_18.test(evt.detail.href)){
_17=evt.detail.href.match(_18)[1];
}
this.transition({"viewId":_17,opts:_2.mixin({},evt.detail),data:evt.detail.data});
},_addTransitionEventToWaitingQueue:function(_19){
if(_19.defaultView&&this.waitingQueue.length>0){
var _1a=false;
for(var i=0;i<this.waitingQueue.length;i++){
var evt=this.waitingQueue[i];
if(!evt.defaultView){
this.waitingQueue.splice(i,0,_19);
_1a=true;
break;
}
}
if(!_1a){
this.waitingQueue.push(_19);
}
}else{
this.waitingQueue.push(_19);
}
},proceedTransition:function(_1b){
var F=_b+":proceedTransition";
if(this.proceeding){
this._addTransitionEventToWaitingQueue(_1b);
this.app.log(F+" added this event to waitingQueue",_1b);
this.processingQueue=false;
return;
}
this.app.log(F+" this.waitingQueue.length ="+this.waitingQueue.length+" this.processingQueue="+this.processingQueue);
if(this.waitingQueue.length>0&&!this.processingQueue){
this.processingQueue=true;
this._addTransitionEventToWaitingQueue(_1b);
this.app.log(F+" added this event to waitingQueue passed proceeding",_1b);
_1b=this.waitingQueue.shift();
this.app.log(F+" shifted waitingQueue to process",_1b);
}
this.proceeding=true;
this.app.log(F+" calling trigger load",_1b);
if(!_1b.opts){
_1b.opts={};
}
var _1c=_1b.params||_1b.opts.params;
this.app.emit("app-load",{"viewId":_1b.viewId,"params":_1c,"forceTransitionNone":_1b.forceTransitionNone,"callback":_2.hitch(this,function(_1d,_1e){
if(_1d){
this.proceeding=false;
this.processingQueue=true;
var _1f=(_1e)?this.waitingQueue.shift():this.waitingQueue.pop();
if(_1f){
this.proceedTransition(_1f);
}
}else{
var _20=this._doTransition(_1b.viewId,_1b.opts,_1c,_1b.opts.data,this.app,_1b._removeView,_1b._doResize,_1b.forceTransitionNone);
_6(_20,_2.hitch(this,function(){
this.proceeding=false;
this.processingQueue=true;
var _21=this.waitingQueue.shift();
if(_21){
this.proceedTransition(_21);
}
}));
}
})});
},_getTransition:function(_22,_23,_24,_25,_26){
if(_26){
return "none";
}
var _27=_23;
var _28=null;
if(_22){
_28=_22.transition;
}
if(!_28&&_27.views[_24]){
_28=_27.views[_24].transition;
}
if(!_28){
_28=_27.transition;
}
var _29=(_22&&_22.defaultTransition)?_22.defaultTransition:_27.defaultTransition;
while(!_28&&_27.parent){
_27=_27.parent;
_28=_27.transition;
if(!_29){
_29=_27.defaultTransition;
}
}
return _28||_25.transition||_29||"none";
},_getParamsForView:function(_2a,_2b){
var _2c={};
for(var _2d in _2b){
var _2e=_2b[_2d];
if(_2.isObject(_2e)){
if(_2d==_2a){
_2c=_2.mixin(_2c,_2e);
}
}else{
if(_2d&&_2e!=null){
_2c[_2d]=_2b[_2d];
}
}
}
return _2c;
},_doTransition:function(_2f,_30,_31,_32,_33,_34,_35,_36,_37){
var F=_b+":_doTransition";
if(!_33){
throw Error("view parent not found in transition.");
}
this.app.log(F+" transitionTo=[",_2f,"], removeView=[",_34,"] parent.name=[",_33.name,"], opts=",_30);
var _38,_39,_3a,_3b;
if(_2f){
_38=_2f.split(",");
}else{
_38=_33.defaultView.split(",");
}
_39=_38.shift();
_3a=_38.join(",");
_3b=_33.children[_33.id+"_"+_39];
if(!_3b){
if(_34){
this.app.log(F+" called with removeView true, but that view is not available to remove");
return;
}
throw Error("child view must be loaded before transition.");
}
if(!_3a&&_3b.defaultView){
_3a=_3b.defaultView;
}
var _3c=[_3b||_33];
if(_3a){
_3c=this._getNextSubViewArray(_3a,_3b,_33);
}
var _3d=_9.getSelectedChild(_33,_3b.constraint);
var _3e=this._getCurrentSubViewArray(_33,_3c,_34);
var _3f=this._getNamesFromArray(_3e,false);
var _40=this._getNamesFromArray(_3c,true);
_3b.params=this._getParamsForView(_3b.name,_31);
if(_34){
if(_3b!==_3d){
this.app.log(F+" called with removeView true, but that view is not available to remove");
return;
}
this.app.log(_c,F,"Transition Remove current From=["+_3f+"]");
_3b=null;
}
if(_40==_3f&&_3b==_3d){
this.app.log(_c,F,"Transition current and next DO MATCH From=["+_3f+"] TO=["+_40+"]");
this._handleMatchingViews(_3c,_3b,_3d,_33,_32,_34,_35,_3a,_3f,_39,_36,_30);
}else{
this.app.log(_c,F,"Transition current and next DO NOT MATCH From=["+_3f+"] TO=["+_40+"]");
if(!_34&&_3b){
var _41=this.nextLastSubChildMatch||_3b;
var _42=false;
for(var i=_3c.length-1;i>=0;i--){
var v=_3c[i];
if(_42||v.id==_41.id){
_42=true;
if(!v._needsResize&&v.domNode){
this.app.log(_c,F," setting domStyle visibility hidden for v.id=["+v.id+"], display=["+v.domNode.style.display+"], visibility=["+v.domNode.style.visibility+"]");
this._setViewVisible(v,false);
}
}
}
}
if(_3d&&_3d._active){
this._handleBeforeDeactivateCalls(_3e,this.nextLastSubChildMatch||_3b,_3d,_32,_3a);
}
if(_3b){
this.app.log(F+" calling _handleBeforeActivateCalls next name=[",_3b.name,"], parent.name=[",_3b.parent.name,"]");
this._handleBeforeActivateCalls(_3c,this.currentLastSubChildMatch||_3d,_32,_3a);
}
if(!_34){
var _41=this.nextLastSubChildMatch||_3b;
var _43=this._getTransition(_41,_33,_39,_30,_36);
this.app.log(F+" calling _handleLayoutAndResizeCalls trans="+_43);
this._handleLayoutAndResizeCalls(_3c,_34,_35,_3a,_36,_43);
}else{
for(var i=0;i<_3c.length;i++){
var v=_3c[i];
this.app.log(_c,F,"setting visibility visible for v.id=["+v.id+"]");
if(v.domNode){
this.app.log(_c,F,"  setting domStyle for removeView visibility visible for v.id=["+v.id+"], display=["+v.domNode.style.display+"]");
this._setViewVisible(v,true);
}
}
}
var _44=true;
if(_a&&(!_37||this.currentLastSubChildMatch!=null)&&this.currentLastSubChildMatch!==_3b){
_44=this._handleTransit(_3b,_33,this.currentLastSubChildMatch,_30,_39,_34,_36,_35);
}
_6(_44,_2.hitch(this,function(){
if(_3b){
this.app.log(F+" back from transit for next ="+_3b.name);
}
if(_34){
var _45=this.nextLastSubChildMatch||_3b;
var _46=this._getTransition(_45,_33,_39,_30,_36);
this._handleLayoutAndResizeCalls(_3c,_34,_35,_3a,_36,_46);
}
this._handleAfterDeactivateCalls(_3e,this.nextLastSubChildMatch||_3b,_3d,_32,_3a);
this._handleAfterActivateCalls(_3c,_34,this.currentLastSubChildMatch||_3d,_32,_3a);
}));
return _44;
}
},_handleMatchingViews:function(_47,_48,_49,_4a,_4b,_4c,_4d,_4e,_4f,_50,_51,_52){
var F=_b+":_handleMatchingViews";
this._handleBeforeDeactivateCalls(_47,this.nextLastSubChildMatch||_48,_49,_4b,_4e);
this._handleAfterDeactivateCalls(_47,this.nextLastSubChildMatch||_48,_49,_4b,_4e);
this._handleBeforeActivateCalls(_47,this.currentLastSubChildMatch||_49,_4b,_4e);
var _53=this.nextLastSubChildMatch||_48;
var _54=this._getTransition(_53,_4a,_50,_52,_51);
this._handleLayoutAndResizeCalls(_47,_4c,_4d,_4e,_54);
this._handleAfterActivateCalls(_47,_4c,this.currentLastSubChildMatch||_49,_4b,_4e);
},_handleBeforeDeactivateCalls:function(_55,_56,_57,_58,_59){
var F=_b+":_handleBeforeDeactivateCalls";
if(_57._active){
for(var i=_55.length-1;i>=0;i--){
var v=_55[i];
if(v&&v.beforeDeactivate&&v._active){
this.app.log(_c,F,"beforeDeactivate for v.id="+v.id);
v.beforeDeactivate(_56,_58);
}
}
}
},_handleAfterDeactivateCalls:function(_5a,_5b,_5c,_5d,_5e){
var F=_b+":_handleAfterDeactivateCalls";
if(_5c&&_5c._active){
for(var i=0;i<_5a.length;i++){
var v=_5a[i];
if(v&&v.beforeDeactivate&&v._active){
this.app.log(_c,F,"afterDeactivate for v.id="+v.id);
v.afterDeactivate(_5b,_5d);
v._active=false;
}
}
}
},_handleBeforeActivateCalls:function(_5f,_60,_61,_62){
var F=_b+":_handleBeforeActivateCalls";
for(var i=_5f.length-1;i>=0;i--){
var v=_5f[i];
this.app.log(_c,F,"beforeActivate for v.id="+v.id);
v.beforeActivate(_60,_61);
}
},_handleLayoutAndResizeCalls:function(_63,_64,_65,_66,_67,_68){
var F=_b+":_handleLayoutAndResizeCalls";
var _69=_64;
for(var i=0;i<_63.length;i++){
var v=_63[i];
this.app.log(_c,F,"emit layoutView v.id=["+v.id+"] removeView=["+_69+"]");
this.app.emit("app-layoutView",{"parent":v.parent,"view":v,"removeView":_69,"doResize":false,"transition":_68,"currentLastSubChildMatch":this.currentLastSubChildMatch});
_69=false;
}
if(_65){
this.app.log(_c,F,"emit doResize called");
this.app.emit("app-resize");
if(_68=="none"){
this._showSelectedChildren(this.app);
}
}
},_showSelectedChildren:function(w){
var F=_b+":_showSelectedChildren";
this.app.log(_c,F," setting domStyle visibility visible for w.id=["+w.id+"], display=["+w.domNode.style.display+"], visibility=["+w.domNode.style.visibility+"]");
this._setViewVisible(w,true);
w._needsResize=false;
for(var _6a in w.selectedChildren){
if(w.selectedChildren[_6a]&&w.selectedChildren[_6a].domNode){
this.app.log(_c,F," calling _showSelectedChildren for w.selectedChildren[hash].id="+w.selectedChildren[_6a].id);
this._showSelectedChildren(w.selectedChildren[_6a]);
}
}
},_setViewVisible:function(v,_6b){
if(_6b){
_7.set(v.domNode,"visibility","visible");
}else{
_7.set(v.domNode,"visibility","hidden");
}
},_handleAfterActivateCalls:function(_6c,_6d,_6e,_6f,_70){
var F=_b+":_handleAfterActivateCalls";
var _71=0;
if(_6d&&_6c.length>1){
_71=1;
}
for(var i=_71;i<_6c.length;i++){
var v=_6c[i];
if(v.afterActivate){
this.app.log(_c,F,"afterActivate for v.id="+v.id);
v.afterActivate(_6e,_6f);
v._active=true;
}
}
},_getNextSubViewArray:function(_72,_73,_74){
var F=_b+":_getNextSubViewArray";
var _75=[];
var p=_73||_74;
if(_72){
_75=_72.split(",");
}
var _76=[p];
for(var i=0;i<_75.length;i++){
toId=_75[i];
var v=p.children[p.id+"_"+toId];
if(v){
_76.push(v);
p=v;
}
}
_76.reverse();
return _76;
},_getCurrentSubViewArray:function(_77,_78,_79){
var F=_b+":_getCurrentSubViewArray";
var _7a=[];
var _7b,_7c,_7d;
var p=_77;
this.currentLastSubChildMatch=null;
this.nextLastSubChildMatch=null;
for(var i=_78.length-1;i>=0;i--){
_7b=_78[i].constraint;
_7c=typeof (_7b);
_7d=(_7c=="string"||_7c=="number")?_7b:_7b.__hash;
if(p&&p.selectedChildren&&p.selectedChildren[_7d]){
if(p.selectedChildren[_7d]==_78[i]){
this.currentLastSubChildMatch=p.selectedChildren[_7d];
this.nextLastSubChildMatch=_78[i];
_7a.push(this.currentLastSubChildMatch);
p=this.currentLastSubChildMatch;
}else{
this.currentLastSubChildMatch=p.selectedChildren[_7d];
_7a.push(this.currentLastSubChildMatch);
this.nextLastSubChildMatch=_78[i];
if(!_79){
var _7e=_9.getAllSelectedChildren(this.currentLastSubChildMatch);
_7a=_7a.concat(_7e);
}
break;
}
}else{
this.currentLastSubChildMatch=null;
this.nextLastSubChildMatch=_78[i];
break;
}
}
if(_79){
var _7e=_9.getAllSelectedChildren(p);
_7a=_7a.concat(_7e);
}
return _7a;
},_getNamesFromArray:function(_7f,_80){
var F=_b+":_getNamesFromArray";
var _81="";
if(_80){
for(var i=_7f.length-1;i>=0;i--){
_81=_81?_81+","+_7f[i].name:_7f[i].name;
}
}else{
for(var i=0;i<_7f.length;i++){
_81=_81?_81+","+_7f[i].name:_7f[i].name;
}
}
return _81;
},_handleTransit:function(_82,_83,_84,_85,_86,_87,_88,_89){
var F=_b+":_handleTransit";
var _8a=this.nextLastSubChildMatch||_82;
var _8b=_2.mixin({},_85);
_8b=_2.mixin({},_8b,{reverse:(_8b.reverse||_8b.transitionDir===-1)?true:false,transition:this._getTransition(_8a,_83,_86,_8b,_88)});
if(_87){
_8a=null;
}
if(_84){
this.app.log(_c,F,"transit FROM currentLastSubChild.id=["+_84.id+"]");
}
if(_8a){
if(_8b.transition!=="none"){
if(!_89&&_8a._needsResize){
this.app.log(_c,F,"emit doResize called from _handleTransit");
this.app.emit("app-resize");
}
this.app.log(_c,F,"  calling _showSelectedChildren for w3.id=["+_8a.id+"], display=["+_8a.domNode.style.display+"], visibility=["+_8a.domNode.style.visibility+"]");
this._showSelectedChildren(this.app);
}
this.app.log(_c,F,"transit TO nextLastSubChild.id=["+_8a.id+"] transition=["+_8b.transition+"]");
}else{
this._showSelectedChildren(this.app);
}
return _a(_84&&_84.domNode,_8a&&_8a.domNode,_8b);
}});
});
