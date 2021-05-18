//>>built
require({cache:{"url:dojox/layout/resources/ScrollPane.html":"<div class=\"dojoxScrollWindow\" dojoAttachEvent=\"onmouseenter: _enter, onmouseleave: _leave\">\n    <div class=\"dojoxScrollWrapper\" style=\"${style}\" dojoAttachPoint=\"wrapper\" dojoAttachEvent=\"onmousemove: _calc\">\n\t<div class=\"dojoxScrollPane\" dojoAttachPoint=\"containerNode\"></div>\n    </div>\n    <div dojoAttachPoint=\"helper\" class=\"dojoxScrollHelper\"><span class=\"helperInner\">|</span></div>\n</div>"}});
define("dojox/layout/ScrollPane",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/html","dojo/_base/fx","dijit/_Templated","dijit/layout/ContentPane","dojo/dom-class","dojo/text!./resources/ScrollPane.html"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.layout.ScrollPane");
var _9=_2("dojox.layout.ScrollPane",[_6,_5],{_line:null,_lo:null,_offset:15,orientation:"vertical",autoHide:true,templateString:_8,resize:function(_a){
if(_a){
if(_a.h){
_3.style(this.domNode,"height",_a.h+"px");
}
if(_a.w){
_3.style(this.domNode,"width",_a.w+"px");
}
}
var _b=this._dir,_c=this._vertical,_d=this.containerNode[(_c?"scrollHeight":"scrollWidth")];
_3.style(this.wrapper,this._dir,this.domNode.style[this._dir]);
this._lo=_3.coords(this.wrapper,true);
this._size=Math.max(0,_d-this._lo[(_c?"h":"w")]);
if(!this._size){
this.helper.style.display="none";
this.wrapper[this._scroll]=0;
return;
}else{
this.helper.style.display="";
}
this._line=new _4._Line(0-this._offset,this._size+(this._offset*2));
var u=this._lo[(_c?"h":"w")],r=Math.min(1,u/_d),s=u*r,c=Math.floor(u-(u*r));
this._helpLine=new _4._Line(0,c);
_3.style(this.helper,_b,Math.floor(s)+"px");
},postCreate:function(){
this.inherited(arguments);
if(this.autoHide){
this._showAnim=_4._fade({node:this.helper,end:0.5,duration:350});
this._hideAnim=_4.fadeOut({node:this.helper,duration:750});
}
this._vertical=(this.orientation=="vertical");
if(!this._vertical){
_7.add(this.containerNode,"dijitInline");
this._dir="width";
this._edge="left";
this._scroll="scrollLeft";
}else{
this._dir="height";
this._edge="top";
this._scroll="scrollTop";
}
if(this._hideAnim){
this._hideAnim.play();
}
_3.style(this.wrapper,"overflow","hidden");
},_set:function(n){
if(!this._size||n==="focused"){
return;
}
this.wrapper[this._scroll]=Math.floor(this._line.getValue(n));
_3.style(this.helper,this._edge,Math.floor(this._helpLine.getValue(n))+"px");
},_calc:function(e){
if(!this._lo){
this.resize();
}
this._set(this._vertical?((e.pageY-this._lo.y)/this._lo.h):((e.pageX-this._lo.x)/this._lo.w));
},_enter:function(e){
if(this._hideAnim){
if(this._hideAnim.status()=="playing"){
this._hideAnim.stop();
}
this._showAnim.play();
}
},_leave:function(e){
if(this._hideAnim){
this._hideAnim.play();
}
}});
return _9;
});
