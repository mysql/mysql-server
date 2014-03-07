//>>built
define("dijit/layout/StackController",["dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/_base/sniff","../focus","../registry","../_Widget","../_TemplatedMixin","../_Container","../form/ToggleButton","dojo/i18n!../nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_2("dijit.layout._StackButton",_c,{tabIndex:"-1",closeButton:false,_setCheckedAttr:function(_e,_f){
this.inherited(arguments);
this.focusNode.removeAttribute("aria-pressed");
},buildRendering:function(evt){
this.inherited(arguments);
(this.focusNode||this.domNode).setAttribute("role","tab");
},onClick:function(){
_7.focus(this.focusNode);
},onClickCloseButton:function(evt){
evt.stopPropagation();
}});
var _10=_2("dijit.layout.StackController",[_9,_a,_b],{baseClass:"dijitStackController",templateString:"<span role='tablist' data-dojo-attach-event='onkeypress'></span>",containerId:"",buttonWidget:_d,constructor:function(){
this.pane2button={};
this.pane2connects={};
this.pane2watches={};
},postCreate:function(){
this.inherited(arguments);
this.subscribe(this.containerId+"-startup","onStartup");
this.subscribe(this.containerId+"-addChild","onAddChild");
this.subscribe(this.containerId+"-removeChild","onRemoveChild");
this.subscribe(this.containerId+"-selectChild","onSelectChild");
this.subscribe(this.containerId+"-containerKeyPress","onContainerKeyPress");
},onStartup:function(_11){
_1.forEach(_11.children,this.onAddChild,this);
if(_11.selected){
this.onSelectChild(_11.selected);
}
},destroy:function(){
for(var _12 in this.pane2button){
this.onRemoveChild(_8.byId(_12));
}
this.inherited(arguments);
},onAddChild:function(_13,_14){
var cls=_5.isString(this.buttonWidget)?_5.getObject(this.buttonWidget):this.buttonWidget;
var _15=new cls({id:this.id+"_"+_13.id,label:_13.title,dir:_13.dir,lang:_13.lang,textDir:_13.textDir,showLabel:_13.showTitle,iconClass:_13.iconClass,closeButton:_13.closable,title:_13.tooltip});
_15.focusNode.setAttribute("aria-selected","false");
var _16=["title","showTitle","iconClass","closable","tooltip"],_17=["label","showLabel","iconClass","closeButton","title"];
this.pane2watches[_13.id]=_1.map(_16,function(_18,idx){
return _13.watch(_18,function(_19,_1a,_1b){
_15.set(_17[idx],_1b);
});
});
this.pane2connects[_13.id]=[this.connect(_15,"onClick",_5.hitch(this,"onButtonClick",_13)),this.connect(_15,"onClickCloseButton",_5.hitch(this,"onCloseButtonClick",_13))];
this.addChild(_15,_14);
this.pane2button[_13.id]=_15;
_13.controlButton=_15;
if(!this._currentChild){
_15.focusNode.setAttribute("tabIndex","0");
_15.focusNode.setAttribute("aria-selected","true");
this._currentChild=_13;
}
if(!this.isLeftToRight()&&_6("ie")&&this._rectifyRtlTabList){
this._rectifyRtlTabList();
}
},onRemoveChild:function(_1c){
if(this._currentChild===_1c){
this._currentChild=null;
}
_1.forEach(this.pane2connects[_1c.id],_5.hitch(this,"disconnect"));
delete this.pane2connects[_1c.id];
_1.forEach(this.pane2watches[_1c.id],function(w){
w.unwatch();
});
delete this.pane2watches[_1c.id];
var _1d=this.pane2button[_1c.id];
if(_1d){
this.removeChild(_1d);
delete this.pane2button[_1c.id];
_1d.destroy();
}
delete _1c.controlButton;
},onSelectChild:function(_1e){
if(!_1e){
return;
}
if(this._currentChild){
var _1f=this.pane2button[this._currentChild.id];
_1f.set("checked",false);
_1f.focusNode.setAttribute("aria-selected","false");
_1f.focusNode.setAttribute("tabIndex","-1");
}
var _20=this.pane2button[_1e.id];
_20.set("checked",true);
_20.focusNode.setAttribute("aria-selected","true");
this._currentChild=_1e;
_20.focusNode.setAttribute("tabIndex","0");
var _21=_8.byId(this.containerId);
_21.containerNode.setAttribute("aria-labelledby",_20.id);
},onButtonClick:function(_22){
if(this._currentChild.id===_22.id){
var _23=this.pane2button[_22.id];
_23.set("checked",true);
}
var _24=_8.byId(this.containerId);
_24.selectChild(_22);
},onCloseButtonClick:function(_25){
var _26=_8.byId(this.containerId);
_26.closeChild(_25);
if(this._currentChild){
var b=this.pane2button[this._currentChild.id];
if(b){
_7.focus(b.focusNode||b.domNode);
}
}
},adjacent:function(_27){
if(!this.isLeftToRight()&&(!this.tabPosition||/top|bottom/.test(this.tabPosition))){
_27=!_27;
}
var _28=this.getChildren();
var _29=_1.indexOf(_28,this.pane2button[this._currentChild.id]);
var _2a=_27?1:_28.length-1;
return _28[(_29+_2a)%_28.length];
},onkeypress:function(e){
if(this.disabled||e.altKey){
return;
}
var _2b=null;
if(e.ctrlKey||!e._djpage){
switch(e.charOrCode){
case _4.LEFT_ARROW:
case _4.UP_ARROW:
if(!e._djpage){
_2b=false;
}
break;
case _4.PAGE_UP:
if(e.ctrlKey){
_2b=false;
}
break;
case _4.RIGHT_ARROW:
case _4.DOWN_ARROW:
if(!e._djpage){
_2b=true;
}
break;
case _4.PAGE_DOWN:
if(e.ctrlKey){
_2b=true;
}
break;
case _4.HOME:
case _4.END:
var _2c=this.getChildren();
if(_2c&&_2c.length){
_2c[e.charOrCode==_4.HOME?0:_2c.length-1].onClick();
}
_3.stop(e);
break;
case _4.DELETE:
if(this._currentChild.closable){
this.onCloseButtonClick(this._currentChild);
}
_3.stop(e);
break;
default:
if(e.ctrlKey){
if(e.charOrCode===_4.TAB){
this.adjacent(!e.shiftKey).onClick();
_3.stop(e);
}else{
if(e.charOrCode=="w"){
if(this._currentChild.closable){
this.onCloseButtonClick(this._currentChild);
}
_3.stop(e);
}
}
}
}
if(_2b!==null){
this.adjacent(_2b).onClick();
_3.stop(e);
}
}
},onContainerKeyPress:function(_2d){
_2d.e._djpage=_2d.page;
this.onkeypress(_2d.e);
}});
_10.StackButton=_d;
return _10;
});
