//>>built
require({cache:{"url:dojox/widget/Dialog/Dialog.html":"<div class=\"dojoxDialog\" tabindex=\"-1\" role=\"dialog\" aria-labelledby=\"${id}_title\">\n\t<div dojoAttachPoint=\"titleBar\" class=\"dojoxDialogTitleBar\">\n\t\t<span dojoAttachPoint=\"titleNode\" class=\"dojoxDialogTitle\" id=\"${id}_title\">${title}</span>\n\t</div>\n\t<div dojoAttachPoint=\"dojoxDialogWrapper\">\n\t\t<div dojoAttachPoint=\"containerNode\" class=\"dojoxDialogPaneContent\"></div>\n\t</div>\n\t<div dojoAttachPoint=\"closeButtonNode\" class=\"dojoxDialogCloseIcon\" dojoAttachEvent=\"onclick: onCancel\">\n\t\t\t<span dojoAttachPoint=\"closeText\" class=\"closeText\">x</span>\n\t</div>\n</div>\n"}});
define("dojox/widget/Dialog",["dojo","dojox","dojo/text!./Dialog/Dialog.html","dijit/Dialog","dojo/window","dojox/fx","./DialogSimple"],function(_1,_2,_3){
_1.getObject("widget",true,_2);
_1.declare("dojox.widget.Dialog",_2.widget.DialogSimple,{templateString:_3,sizeToViewport:false,viewportPadding:35,dimensions:null,easing:null,sizeDuration:dijit._defaultDuration,sizeMethod:"chain",showTitle:false,draggable:false,modal:false,constructor:function(_4,_5){
this.easing=_4.easing||_1._defaultEasing;
this.dimensions=_4.dimensions||[300,300];
},_setup:function(){
this.inherited(arguments);
if(!this._alreadyInitialized){
this._navIn=_1.fadeIn({node:this.closeButtonNode});
this._navOut=_1.fadeOut({node:this.closeButtonNode});
if(!this.showTitle){
_1.addClass(this.domNode,"dojoxDialogNoTitle");
}
}
},layout:function(e){
this._setSize();
this.inherited(arguments);
},_setSize:function(){
this._vp=_1.window.getBox();
var tc=this.containerNode,_6=this.sizeToViewport;
return this._displaysize={w:_6?tc.scrollWidth:this.dimensions[0],h:_6?tc.scrollHeight:this.dimensions[1]};
},show:function(){
if(this.open){
return;
}
this._setSize();
_1.style(this.closeButtonNode,"opacity",0);
_1.style(this.domNode,{overflow:"hidden",opacity:0,width:"1px",height:"1px"});
_1.style(this.containerNode,{opacity:0,overflow:"hidden"});
this.inherited(arguments);
if(this.modal){
this._modalconnects.push(_1.connect(_1.body(),"onkeypress",function(e){
if(e.charOrCode==_1.keys.ESCAPE){
_1.stopEvent(e);
}
}));
}else{
this._modalconnects.push(_1.connect(dijit._underlay.domNode,"onclick",this,"onCancel"));
}
this._modalconnects.push(_1.connect(this.domNode,"onmouseenter",this,"_handleNav"));
this._modalconnects.push(_1.connect(this.domNode,"onmouseleave",this,"_handleNav"));
},_handleNav:function(e){
var _7="_navOut",_8="_navIn",_9=(e.type=="mouseout"?_8:_7),_a=(e.type=="mouseout"?_7:_8);
this[_9].stop();
this[_a].play();
},_position:function(){
if(!this._started){
return;
}
if(this._sizing){
this._sizing.stop();
this.disconnect(this._sizingConnect);
delete this._sizing;
}
this.inherited(arguments);
if(!this.open){
_1.style(this.containerNode,"opacity",0);
}
var _b=this.viewportPadding*2;
var _c={node:this.domNode,duration:this.sizeDuration||dijit._defaultDuration,easing:this.easing,method:this.sizeMethod};
var ds=this._displaysize||this._setSize();
_c["width"]=ds.w=(ds.w+_b>=this._vp.w||this.sizeToViewport)?this._vp.w-_b:ds.w;
_c["height"]=ds.h=(ds.h+_b>=this._vp.h||this.sizeToViewport)?this._vp.h-_b:ds.h;
this._sizing=_2.fx.sizeTo(_c);
this._sizingConnect=this.connect(this._sizing,"onEnd","_showContent");
this._sizing.play();
},_showContent:function(e){
var _d=this.containerNode;
_1.style(this.domNode,{overflow:"visible",opacity:1});
_1.style(this.closeButtonNode,"opacity",1);
_1.style(_d,{height:this._displaysize.h-this.titleNode.offsetHeight+"px",width:this._displaysize.w+"px",overflow:"auto"});
_1.anim(_d,{opacity:1});
}});
return _2.widget.Dialog;
});
