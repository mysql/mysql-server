//>>built
define("dojox/mobile/pageTurningUtils",["dojo/_base/kernel","dojo/_base/array","dojo/_base/connect","dojo/_base/event","dojo/dom-class","dojo/dom-construct","dojo/dom-style","./_css3"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.mobile.pageTurningUtils");
return function(){
this.w=0;
this.h=0;
this.turnfrom="top";
this.page=1;
this.dogear=1;
this.duration=2;
this.alwaysDogeared=false;
this._styleParams={};
this._catalogNode=null;
this._currentPageNode=null;
this._transitionEndHandle=null;
this.init=function(w,h,_9,_a,_b,_c,_d){
this.w=w;
this.h=h;
this.turnfrom=_9?_9:this.turnfrom;
this.page=_a?_a:this.page;
this.dogear=typeof _b!=="undefined"?_b:this.dogear;
this.duration=typeof _c!=="undefined"?_c:this.duration;
this.alwaysDogeared=typeof _d!=="undefined"?_d:this.alwaysDogeared;
if(this.turnfrom==="bottom"){
this.alwaysDogeared=true;
}
this._calcStyleParams();
};
this._calcStyleParams=function(){
var _e=Math.tan(58*Math.PI/180),_f=Math.cos(32*Math.PI/180),_10=Math.sin(32*Math.PI/180),_11=Math.tan(32*Math.PI/180),w=this.w,h=this.h,_12=this.page,_13=this.turnfrom,_14=this._styleParams;
var _15=w*_e,Q=_15,fw=Q*_10+Q*_f*_e,fh=_15+w+w/_e,dw=w*0.11*this.dogear,pw=w-dw,_16=pw*_f,cx,cy,dx,dy,fy;
switch(this.turnfrom){
case "top":
cx=fw-_16;
cy=_16*_e;
dx=fw-dw;
dy=cy+pw/_e-7;
fy=cy/_f;
_14.init={page:_8.add({top:-fy+"px",left:(-fw+(_12===2?w:0))+"px",width:fw+"px",height:fh+"px"},{transformOrigin:"100% 0%"}),front:_8.add({width:w+"px",height:h+"px"},{boxShadow:"0 0"}),back:_8.add({width:w+"px",height:h+"px"},{boxShadow:"0 0"}),shadow:{display:"",left:fw+"px",height:h*1.5+"px"}};
_14.turnForward={page:_8.add({},{transform:"rotate(0deg)"}),front:_8.add({},{transform:"translate("+fw+"px,"+fy+"px) rotate(0deg)",transformOrigin:"-110px -18px"}),back:_8.add({},{transform:"translate("+(fw-w)+"px,"+fy+"px) rotate(0deg)",transformOrigin:"0px 0px"})};
_14.turnBackward={page:_8.add({},{transform:"rotate(-32deg)"}),front:_8.add({},{transform:"translate("+cx+"px,"+cy+"px) rotate(32deg)",transformOrigin:"0px 0px"}),back:_8.add({},{transform:"translate("+dx+"px,"+dy+"px) rotate(-32deg)",transformOrigin:"0px 0px"})};
break;
case "bottom":
cx=fw-(h*_10+w*_f)-2;
cy=fh-(h+w/_11)*_f;
dx=fw;
dy=fh-w/_10-h;
fy=fh-w/_11-h;
_14.init={page:_8.add({top:(-fy+50)+"px",left:(-fw+(_12===2?w:0))+"px",width:fw+"px",height:fh+"px"},{transformOrigin:"100% 100%"}),front:_8.add({width:w+"px",height:h+"px"},{boxShadow:"0 0"}),back:_8.add({width:w+"px",height:h+"px"},{boxShadow:"0 0"}),shadow:{display:"none"}};
_14.turnForward={page:_8.add({},{transform:"rotate(0deg)"}),front:_8.add({},{transform:"translate("+fw+"px,"+fy+"px) rotate(0deg)",transformOrigin:"-220px 35px"}),back:_8.add({},{transform:"translate("+(w*2)+"px,"+fy+"px) rotate(0deg)",transformOrigin:"0px 0px"})};
_14.turnBackward={page:_8.add({},{transform:"rotate(32deg)"}),front:_8.add({},{transform:"translate("+cx+"px,"+cy+"px) rotate(-32deg)",transformOrigin:"0px 0px"}),back:_8.add({},{transform:"translate("+dx+"px,"+dy+"px) rotate(0deg)",transformOrigin:"0px 0px"})};
break;
case "left":
cx=-w;
cy=pw/_11-2;
dx=-pw;
dy=fy=pw/_10+dw*_10;
_14.init={page:_8.add({top:-cy+"px",left:w+"px",width:fw+"px",height:fh+"px"},{transformOrigin:"0% 0%"}),front:_8.add({width:w+"px",height:h+"px"},{boxShadow:"0 0"}),back:_8.add({width:w+"px",height:h+"px"},{boxShadow:"0 0"}),shadow:{display:"",left:"-4px",height:((_12===2?h*1.5:h)+50)+"px"}};
_14.turnForward={page:_8.add({},{transform:"rotate(0deg)"}),front:_8.add({},{transform:"translate("+cx+"px,"+cy+"px) rotate(0deg)",transformOrigin:"160px 68px"}),back:_8.add({},{transform:"translate(0px,"+cy+"px) rotate(0deg)",transformOrigin:"0px 0px"})};
_14.turnBackward={page:_8.add({},{transform:"rotate(32deg)"}),front:_8.add({},{transform:"translate("+(-dw)+"px,"+dy+"px) rotate(-32deg)",transformOrigin:"0px 0px"}),back:_8.add({},{transform:"translate("+dx+"px,"+dy+"px) rotate(32deg)",transformOrigin:"top right"})};
break;
}
_14.init.catalog={width:(_12===2?w*2:w)+"px",height:((_12===2?h*1.5:h)+(_13=="top"?0:50))+"px"};
};
this.getChildren=function(_17){
return _2.filter(_17.childNodes,function(n){
return n.nodeType===1;
});
};
this.getPages=function(){
return this._catalogNode?this.getChildren(this._catalogNode):null;
};
this.getCurrentPage=function(){
return this._currentPageNode;
};
this.getIndexOfPage=function(_18,_19){
if(!_19){
_19=this.getPages();
}
for(var i=0;i<_19.length;i++){
if(_18===_19[i]){
return i;
}
}
return -1;
};
this.getNextPage=function(_1a){
for(var n=_1a.nextSibling;n;n=n.nextSibling){
if(n.nodeType===1){
return n;
}
}
return null;
};
this.getPreviousPage=function(_1b){
for(var n=_1b.previousSibling;n;n=n.previousSibling){
if(n.nodeType===1){
return n;
}
}
return null;
};
this.isPageTurned=function(_1c){
return _1c.style[_8.name("transform")]=="rotate(0deg)";
};
this._onPageTurned=function(e){
_4.stop(e);
if(_5.contains(e.target,"mblPageTurningPage")){
this.onPageTurned(e.target);
}
};
this.onPageTurned=function(){
};
this.initCatalog=function(_1d){
if(this._catalogNode!=_1d){
if(this._transitionEndHandle){
_3.disconnect(this._transitionEndHandle);
}
this._transitionEndHandle=_3.connect(_1d,_8.name("transitionEnd"),this,"_onPageTurned");
this._catalogNode=_1d;
}
_5.add(_1d,"mblPageTurningCatalog");
_7.set(_1d,this._styleParams.init.catalog);
var _1e=this.getPages();
_2.forEach(_1e,function(_1f){
this.initPage(_1f);
},this);
this.resetCatalog();
};
this._getBaseZIndex=function(){
return this._catalogNode.style.zIndex||0;
};
this.resetCatalog=function(){
var _20=this.getPages(),len=_20.length,_21=this._getBaseZIndex();
for(var i=len-1;i>=0;i--){
var _22=_20[i];
this.showDogear(_22);
if(this.isPageTurned(_22)){
_22.style.zIndex=_21+len+1;
}else{
_22.style.zIndex=_21+len-i;
!this.alwaysDogeared&&this.hideDogear(_22);
this._currentPageNode=_22;
}
}
if(!this.alwaysDogeared&&this._currentPageNode!=_20[len-1]){
this.showDogear(this._currentPageNode);
}
};
this.initPage=function(_23,dir){
var _24=this.getChildren(_23);
while(_24.length<3){
_23.appendChild(_6.create("div",null));
_24=this.getChildren(_23);
}
var _25=!_5.contains(_23,"mblPageTurningPage");
_5.add(_23,"mblPageTurningPage");
_5.add(_24[0],"mblPageTurningFront");
_5.add(_24[1],"mblPageTurningBack");
_5.add(_24[2],"mblPageTurningShadow");
var p=this._styleParams.init;
_7.set(_23,p.page);
_7.set(_24[0],p.front);
_7.set(_24[1],p.back);
p.shadow&&_7.set(_24[2],p.shadow);
if(!dir){
if(_25&&this._currentPageNode){
var _26=this.getPages();
dir=this.getIndexOfPage(_23)<this.getIndexOfPage(this._currentPageNode)?1:-1;
}else{
dir=this.isPageTurned(_23)?1:-1;
}
}
this._turnPage(_23,dir,0);
};
this.turnToNext=function(_27){
var _28=this.getNextPage(this._currentPageNode);
if(_28){
this._turnPage(this._currentPageNode,1,_27);
this._currentPageNode=_28;
}
};
this.turnToPrev=function(_29){
var _2a=this.getPreviousPage(this._currentPageNode);
if(_2a){
this._turnPage(_2a,-1,_29);
this._currentPageNode=_2a;
}
};
this.goTo=function(_2b){
var _2c=this.getPages();
if(this._currentPageNode===_2c[_2b]||_2c.length<=_2b){
return;
}
var _2d=_2b<this.getIndexOfPage(this._currentPageNode,_2c);
while(this._currentPageNode!==_2c[_2b]){
_2d?this.turnToPrev(0):this.turnToNext(0);
}
};
this._turnPage=function(_2e,dir,_2f){
var _30=this.getChildren(_2e),d=((typeof _2f!=="undefined")?_2f:this.duration)+"s",p=(dir===1)?this._styleParams.turnForward:this._styleParams.turnBackward;
p.page[_8.name("transitionDuration")]=d;
_7.set(_2e,p.page);
p.front[_8.name("transitionDuration")]=d;
_7.set(_30[0],p.front);
p.back[_8.name("transitionDuration")]=d;
_7.set(_30[1],p.back);
var _31=this.getPages(),_32=this.getNextPage(_2e),len=_31.length,_33=this._getBaseZIndex();
if(dir===1){
_2e.style.zIndex=_33+len+1;
if(!this.alwaysDogeared&&_32&&this.getNextPage(_32)){
this.showDogear(_32);
}
}else{
if(_32){
_32.style.zIndex=_33+len-this.getIndexOfPage(_32,_31);
!this.alwaysDogeared&&this.hideDogear(_32);
}
}
};
this.showDogear=function(_34){
var _35=this.getChildren(_34);
_7.set(_34,"overflow","");
_35[1]&&_7.set(_35[1],"display","");
_35[2]&&_7.set(_35[2],"display",this.turnfrom==="bottom"?"none":"");
};
this.hideDogear=function(_36){
if(this.turnfrom==="bottom"){
return;
}
var _37=this.getChildren(_36);
_7.set(_36,"overflow","visible");
_37[1]&&_7.set(_37[1],"display","none");
_37[2]&&_7.set(_37[2],"display","none");
};
};
});
