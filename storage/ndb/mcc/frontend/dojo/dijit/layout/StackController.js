//>>built
define("dijit/layout/StackController",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/on","../focus","../registry","../_Widget","../_TemplatedMixin","../_Container","../form/ToggleButton","dojo/i18n!../nls/common"],function(_1,_2,_3,_4,_5,_6,on,_7,_8,_9,_a,_b,_c){
var _d=_2("dijit.layout._StackButton",_c,{tabIndex:"-1",closeButton:false,_aria_attr:"aria-selected",buildRendering:function(_e){
this.inherited(arguments);
(this.focusNode||this.domNode).setAttribute("role","tab");
}});
var _f=_2("dijit.layout.StackController",[_9,_a,_b],{baseClass:"dijitStackController",templateString:"<span role='tablist' data-dojo-attach-event='onkeypress'></span>",containerId:"",buttonWidget:_d,buttonWidgetCloseClass:"dijitStackCloseButton",constructor:function(_10){
this.pane2button={};
},postCreate:function(){
this.inherited(arguments);
this.subscribe(this.containerId+"-startup","onStartup");
this.subscribe(this.containerId+"-addChild","onAddChild");
this.subscribe(this.containerId+"-removeChild","onRemoveChild");
this.subscribe(this.containerId+"-selectChild","onSelectChild");
this.subscribe(this.containerId+"-containerKeyPress","onContainerKeyPress");
this.connect(this.containerNode,"click",function(evt){
var _11=_8.getEnclosingWidget(evt.target);
if(_11!=this.containerNode&&!_11.disabled&&_11.page){
for(var _12=evt.target;_12!==this.containerNode;_12=_12.parentNode){
if(_3.contains(_12,this.buttonWidgetCloseClass)){
this.onCloseButtonClick(_11.page);
break;
}else{
if(_12==_11.domNode){
this.onButtonClick(_11.page);
break;
}
}
}
}
});
},onStartup:function(_13){
_1.forEach(_13.children,this.onAddChild,this);
if(_13.selected){
this.onSelectChild(_13.selected);
}
var _14=_8.byId(this.containerId).containerNode,_15=this.pane2button,_16={"title":"label","showtitle":"showLabel","iconclass":"iconClass","closable":"closeButton","tooltip":"title","disabled":"disabled"},_17=function(_18,_19){
return on(_14,"attrmodified-"+_18,function(evt){
var _1a=_15[evt.detail&&evt.detail.widget&&evt.detail.widget.id];
if(_1a){
_1a.set(_19,evt.detail.newValue);
}
});
};
for(var _1b in _16){
this.own(_17(_1b,_16[_1b]));
}
},destroy:function(){
for(var _1c in this.pane2button){
this.onRemoveChild(_8.byId(_1c));
}
this.inherited(arguments);
},onAddChild:function(_1d,_1e){
var Cls=_6.isString(this.buttonWidget)?_6.getObject(this.buttonWidget):this.buttonWidget;
var _1f=new Cls({id:this.id+"_"+_1d.id,name:this.id+"_"+_1d.id,label:_1d.title,disabled:_1d.disabled,ownerDocument:this.ownerDocument,dir:_1d.dir,lang:_1d.lang,textDir:_1d.textDir,showLabel:_1d.showTitle,iconClass:_1d.iconClass,closeButton:_1d.closable,title:_1d.tooltip,page:_1d});
this.addChild(_1f,_1e);
this.pane2button[_1d.id]=_1f;
_1d.controlButton=_1f;
if(!this._currentChild){
this.onSelectChild(_1d);
}
},onRemoveChild:function(_20){
if(this._currentChild===_20){
this._currentChild=null;
}
var _21=this.pane2button[_20.id];
if(_21){
this.removeChild(_21);
delete this.pane2button[_20.id];
_21.destroy();
}
delete _20.controlButton;
},onSelectChild:function(_22){
if(!_22){
return;
}
if(this._currentChild){
var _23=this.pane2button[this._currentChild.id];
_23.set("checked",false);
_23.focusNode.setAttribute("tabIndex","-1");
}
var _24=this.pane2button[_22.id];
_24.set("checked",true);
this._currentChild=_22;
_24.focusNode.setAttribute("tabIndex","0");
var _25=_8.byId(this.containerId);
},onButtonClick:function(_26){
var _27=this.pane2button[_26.id];
_7.focus(_27.focusNode);
if(this._currentChild&&this._currentChild.id===_26.id){
_27.set("checked",true);
}
var _28=_8.byId(this.containerId);
_28.selectChild(_26);
},onCloseButtonClick:function(_29){
var _2a=_8.byId(this.containerId);
_2a.closeChild(_29);
if(this._currentChild){
var b=this.pane2button[this._currentChild.id];
if(b){
_7.focus(b.focusNode||b.domNode);
}
}
},adjacent:function(_2b){
if(!this.isLeftToRight()&&(!this.tabPosition||/top|bottom/.test(this.tabPosition))){
_2b=!_2b;
}
var _2c=this.getChildren();
var idx=_1.indexOf(_2c,this.pane2button[this._currentChild.id]),_2d=_2c[idx];
var _2e;
do{
idx=(idx+(_2b?1:_2c.length-1))%_2c.length;
_2e=_2c[idx];
}while(_2e.disabled&&_2e!=_2d);
return _2e;
},onkeypress:function(e){
if(this.disabled||e.altKey){
return;
}
var _2f=null;
if(e.ctrlKey||!e._djpage){
switch(e.charOrCode){
case _5.LEFT_ARROW:
case _5.UP_ARROW:
if(!e._djpage){
_2f=false;
}
break;
case _5.PAGE_UP:
if(e.ctrlKey){
_2f=false;
}
break;
case _5.RIGHT_ARROW:
case _5.DOWN_ARROW:
if(!e._djpage){
_2f=true;
}
break;
case _5.PAGE_DOWN:
if(e.ctrlKey){
_2f=true;
}
break;
case _5.HOME:
var _30=this.getChildren();
for(var idx=0;idx<_30.length;idx++){
var _31=_30[idx];
if(!_31.disabled){
this.onButtonClick(_31.page);
break;
}
}
_4.stop(e);
break;
case _5.END:
var _30=this.getChildren();
for(var idx=_30.length-1;idx>=0;idx--){
var _31=_30[idx];
if(!_31.disabled){
this.onButtonClick(_31.page);
break;
}
}
_4.stop(e);
break;
case _5.DELETE:
if(this._currentChild.closable){
this.onCloseButtonClick(this._currentChild);
}
_4.stop(e);
break;
default:
if(e.ctrlKey){
if(e.charOrCode===_5.TAB){
this.onButtonClick(this.adjacent(!e.shiftKey).page);
_4.stop(e);
}else{
if(e.charOrCode=="w"){
if(this._currentChild.closable){
this.onCloseButtonClick(this._currentChild);
}
_4.stop(e);
}
}
}
}
if(_2f!==null){
this.onButtonClick(this.adjacent(_2f).page);
_4.stop(e);
}
}
},onContainerKeyPress:function(_32){
_32.e._djpage=_32.page;
this.onkeypress(_32.e);
}});
_f.StackButton=_d;
return _f;
});
