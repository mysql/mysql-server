//>>built
define("dojox/form/_SelectStackMixin",["dojo/_base/lang","dojo/_base/array","dijit/_base/manager","dojo/_base/connect","dojo/_base/declare"],function(_1,_2,_3,_4,_5){
return _5("dojox.form._SelectStackMixin",null,{stackId:"",stackPrefix:"",_paneIdFromOption:function(_6){
return (this.stackPrefix||"")+_6;
},_optionValFromPane:function(id){
var sp=this.stackPrefix;
if(sp&&id.indexOf(sp)===0){
return id.substring(sp.length);
}
return id;
},_togglePane:function(_7,_8){
if(_7._shown!=undefined&&_7._shown==_8){
return;
}
var _9=_2.filter(_7.getDescendants(),"return item.name;");
if(!_8){
_a={};
_2.forEach(_9,function(w){
_a[w.id]=w.disabled;
w.set("disabled",true);
});
_7._savedStates=_a;
}else{
var _a=_7._savedStates||{};
_2.forEach(_9,function(w){
var _b=_a[w.id];
if(_b==undefined){
_b=false;
}
w.set("disabled",_b);
});
delete _7._savedStates;
}
_7._shown=_8;
},_connectTitle:function(_c,_d){
var fx=_1.hitch(this,function(_e){
this.updateOption({value:_d,label:_e});
});
if(_c._setTitleAttr){
this.connect(_c,"_setTitleAttr",fx);
}else{
this.connect(_c,"attr",function(_f,val){
if(_f=="title"&&arguments.length>1){
fx(val);
}
});
}
},onAddChild:function(_10,_11){
if(!this._panes[_10.id]){
this._panes[_10.id]=_10;
var v=this._optionValFromPane(_10.id);
this.addOption({value:v,label:_10.title});
this._connectTitle(_10,v);
}
if(!_10.onShow||!_10.onHide||_10._shown==undefined){
_10.onShow=_1.hitch(this,"_togglePane",_10,true);
_10.onHide=_1.hitch(this,"_togglePane",_10,false);
_10.onHide();
}
},_setValueAttr:function(v){
if("_savedValue" in this){
return;
}
this.inherited(arguments);
},attr:function(_12,_13){
if(_12=="value"&&arguments.length==2&&"_savedValue" in this){
this._savedValue=_13;
}
return this.inherited(arguments);
},onRemoveChild:function(_14){
if(this._panes[_14.id]){
delete this._panes[_14.id];
this.removeOption(this._optionValFromPane(_14.id));
}
},onSelectChild:function(_15){
this._setValueAttr(this._optionValFromPane(_15.id));
},onStartup:function(_16){
var _17=_16.selected;
this.addOption(_2.filter(_2.map(_16.children,function(c){
var v=this._optionValFromPane(c.id);
this._connectTitle(c,v);
var _18=null;
if(!this._panes[c.id]){
this._panes[c.id]=c;
_18={value:v,label:c.title};
}
if(!c.onShow||!c.onHide||c._shown==undefined){
c.onShow=_1.hitch(this,"_togglePane",c,true);
c.onHide=_1.hitch(this,"_togglePane",c,false);
c.onHide();
}
if("_savedValue" in this&&v===this._savedValue){
_17=c;
}
return _18;
},this),function(i){
return i;
}));
var _19=this;
var fx=function(){
delete _19._savedValue;
_19.onSelectChild(_17);
if(!_17._shown){
_19._togglePane(_17,true);
}
};
if(_17!==_16.selected){
var _1a=_3.byId(this.stackId);
var c=this.connect(_1a,"_showChild",function(sel){
this.disconnect(c);
fx();
});
}else{
fx();
}
},postMixInProperties:function(){
this._savedValue=this.value;
this.inherited(arguments);
this.connect(this,"onChange","_handleSelfOnChange");
},postCreate:function(){
this.inherited(arguments);
this._panes={};
this._subscriptions=[_4.subscribe(this.stackId+"-startup",this,"onStartup"),_4.subscribe(this.stackId+"-addChild",this,"onAddChild"),_4.subscribe(this.stackId+"-removeChild",this,"onRemoveChild"),_4.subscribe(this.stackId+"-selectChild",this,"onSelectChild")];
var _1b=_3.byId(this.stackId);
if(_1b&&_1b._started){
this.onStartup({children:_1b.getChildren(),selected:_1b.selectedChildWidget});
}
},destroy:function(){
_2.forEach(this._subscriptions,_4.unsubscribe);
delete this._panes;
this.inherited("destroy",arguments);
},_handleSelfOnChange:function(val){
var _1c=this._panes[this._paneIdFromOption(val)];
if(_1c){
var s=_3.byId(this.stackId);
if(_1c==s.selectedChildWidget){
s._transition(_1c);
}else{
s.selectChild(_1c);
}
}
}});
});
