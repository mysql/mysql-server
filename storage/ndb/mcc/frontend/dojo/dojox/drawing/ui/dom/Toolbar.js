//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.ui.dom.Toolbar");
_2.deprecated("dojox.drawing.ui.dom.Toolbar","It may not even make it to the 1.4 release.",1.4);
(function(){
_2.declare("dojox.drawing.ui.dom.Toolbar",[],{baseClass:"drawingToolbar",buttonClass:"drawingButton",iconClass:"icon",constructor:function(_4,_5){
_2.addOnLoad(this,function(){
this.domNode=_2.byId(_5);
_2.addClass(this.domNode,this.baseClass);
this.parse();
});
},createIcon:function(_6,_7){
var _8=_7&&_7.setup?_7.setup:{};
if(_8.iconClass){
var _9=_8.iconClass?_8.iconClass:"iconNone";
var _a=_8.tooltip?_8.tooltip:"Tool";
var _b=_2.create("div",{title:_a},_6);
_2.addClass(_b,this.iconClass);
_2.addClass(_b,_9);
_2.connect(_6,"mouseup",function(_c){
_2.stopEvent(_c);
_2.removeClass(_6,"active");
});
_2.connect(_6,"mouseover",function(_d){
_2.stopEvent(_d);
_2.addClass(_6,"hover");
});
_2.connect(_6,"mousedown",this,function(_e){
_2.stopEvent(_e);
_2.addClass(_6,"active");
});
_2.connect(_6,"mouseout",this,function(_f){
_2.stopEvent(_f);
_2.removeClass(_6,"hover");
});
}
},createTool:function(_10){
_10.innerHTML="";
var _11=_2.attr(_10,"tool");
this.toolNodes[_11]=_10;
_2.attr(_10,"tabIndex",1);
var _12=_2.getObject(_11);
this.createIcon(_10,_12);
this.drawing.registerTool(_11,_12);
_2.connect(_10,"mouseup",this,function(evt){
_2.stopEvent(evt);
_2.removeClass(_10,"active");
this.onClick(_11);
});
_2.connect(_10,"mouseover",function(evt){
_2.stopEvent(evt);
_2.addClass(_10,"hover");
});
_2.connect(_10,"mousedown",this,function(evt){
_2.stopEvent(evt);
_2.addClass(_10,"active");
});
_2.connect(_10,"mouseout",this,function(evt){
_2.stopEvent(evt);
_2.removeClass(_10,"hover");
});
},parse:function(){
var _13=_2.attr(this.domNode,"drawingId");
this.drawing=_3.drawing.util.common.byId(_13);
!this.drawing&&console.error("Drawing not found based on 'drawingId' in Toolbar. ");
this.toolNodes={};
var _14;
_2.query(">",this.domNode).forEach(function(_15,i){
_15.className=this.buttonClass;
var _16=_2.attr(_15,"tool");
var _17=_2.attr(_15,"action");
var _18=_2.attr(_15,"plugin");
if(_16){
if(i==0||_2.attr(_15,"selected")=="true"){
_14=_16;
}
this.createTool(_15);
}else{
if(_18){
var p={name:_18,options:{}},opt=_2.attr(_15,"options");
if(opt){
p.options=eval("("+opt+")");
}
p.options.node=_15;
_15.innerHTML="";
this.drawing.addPlugin(p);
this.createIcon(_15,_2.getObject(_2.attr(_15,"plugin")));
}
}
},this);
this.drawing.initPlugins();
_2.connect(this.drawing,"setTool",this,"onSetTool");
this.drawing.setTool(_14);
},onClick:function(_19){
this.drawing.setTool(_19);
},onSetTool:function(_1a){
for(var n in this.toolNodes){
if(n==_1a){
_2.addClass(this.toolNodes[_1a],"selected");
this.toolNodes[_1a].blur();
}else{
_2.removeClass(this.toolNodes[n],"selected");
}
}
}});
})();
});
