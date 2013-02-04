//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_Widget,dijit/_Templated,dojo/fx"],function(_1,_2,_3){
_2.provide("dojox.widget.Pager");
_2.experimental("dojox.widget.Pager");
_2.require("dijit._Widget");
_2.require("dijit._Templated");
_2.require("dojo.fx");
_2.declare("dojox.widget.Pager",[_1._Widget,_1._Templated],{templateString:_2.cache("dojox.widget","Pager/Pager.html","<div dojoAttachPoint=\"pagerContainer\" tabIndex=\"0\" dojoAttachEvent=\"onkeypress: _handleKey, onfocus: _a11yStyle, onblur:_a11yStyle\" class=\"${orientation}PagerContainer\">\n    <div class=\"pagerContainer\">\n\t\t<div dojoAttachPoint=\"pagerContainerStatus\" class=\"${orientation}PagerStatus\"></div>\n\t\t<div dojoAttachPoint=\"pagerContainerView\" class=\"${orientation}PagerView\">\n\t\t    <div dojoAttachPoint=\"pagerItemContainer\"><ul dojoAttachPoint=\"pagerItems\" class=\"pagerItems\"></ul></div>\n\t\t</div>\n\t\t<div dojoAttachPoint=\"pagerContainerPager\" class=\"${orientation}PagerPager\">\n\t\t\t<div tabIndex=\"0\" dojoAttachPoint=\"pagerNext\" class=\"pagerIconContainer\" dojoAttachEvent=\"onclick: _pagerNext\"><img dojoAttachPoint=\"pagerIconNext\" src=\"${iconNext}\" alt=\"Next\" /></div>\n\t\t\t<div tabIndex=\"0\" dojoAttachPoint=\"pagerPrevious\" class=\"pagerIconContainer\" dojoAttachEvent=\"onclick: _pagerPrevious\"><img dojoAttachPoint=\"pagerIconPrevious\" src=\"${iconPrevious}\" alt=\"Previous\" /></div>\n\t\t</div>\n    </div>\n\t<div dojoAttachPoint=\"containerNode\" style=\"display:none\"></div>\n</div>"),iconPage:_2.moduleUrl("dojox.widget","Pager/images/pageInactive.png"),iconPageActive:_2.moduleUrl("dojox.widget","Pager/images/pageActive.png"),store:null,orientation:"horizontal",statusPos:"leading",pagerPos:"center",duration:500,itemSpace:2,resizeChildren:true,itemClass:"dojox.widget._PagerItem",itemsPage:3,postMixInProperties:function(){
var h=(this.orientation=="horizontal");
_2.mixin(this,{_totalPages:0,_currentPage:1,dirClass:"pager"+(h?"Horizontal":"Vertical"),iconNext:_2.moduleUrl("dojox.widget","Pager/images/"+(h?"h":"v")+"Next.png"),iconPrevious:_2.moduleUrl("dojox.widget","Pager/images/"+(h?"h":"v")+"Previous.png")});
},postCreate:function(){
this.inherited(arguments);
this.store.fetch({onComplete:_2.hitch(this,"_init")});
},_a11yStyle:function(e){
_2[(e.type=="focus"?"addClass":"removeClass")](e.target,"pagerFocus");
},_handleKey:function(e){
var dk=_2.keys;
var _4=(e.charCode==dk.SPACE?dk.SPACE:e.keyCode);
switch(_4){
case dk.UP_ARROW:
case dk.RIGHT_ARROW:
case 110:
case 78:
e.preventDefault();
this._pagerNext();
break;
case dk.DOWN_ARROW:
case dk.LEFT_ARROW:
case 112:
case 80:
e.preventDefault();
this._pagerPrevious();
break;
case dk.ENTER:
switch(e.target){
case this.pagerNext:
this._pagerNext();
break;
case this.pagerPrevious:
this._pagerPrevious();
break;
}
break;
}
},_init:function(_5){
this.items=_5;
this._renderPages();
this._renderStatus();
this._renderPager();
},_renderPages:function(){
var _6=this.pagerContainerView;
var _7=(this.orientation=="horizontal");
var _8=_2.style;
if(_7){
var _9=_2.marginBox(this.pagerContainerPager).h;
var _a=_2.marginBox(this.pagerContainerStatus).h;
if(this.pagerPos!="center"){
var _b=_9+_a;
}else{
var _b=_a;
var _c=this.pagerIconNext.width;
var _d=_8(_6,"width");
var _e=_d-(2*_c);
_8(_6,{width:_e+"px",marginLeft:this.pagerIconNext.width+"px",marginRight:this.pagerIconNext.width+"px"});
}
var _f=_8(this.pagerContainer,"height")-_b;
_8(this.pagerContainerView,"height",_f+"px");
var _10=Math.floor(_8(_6,"width")/this.itemsPage);
if(this.statusPos=="trailing"){
if(this.pagerPos!="center"){
_8(_6,"marginTop",_9+"px");
}
_8(_6,"marginBottom",_a+"px");
}else{
_8(_6,"marginTop",_a+"px");
if(this.pagerPos!="center"){
_8(_6,"marginTop",_9+"px");
}
}
}else{
var _11=_2.marginBox(this.pagerContainerPager).w;
var _12=_2.marginBox(this.pagerContainerStatus).w;
var _13=_8(this.pagerContainer,"width");
if(this.pagerPos!="center"){
var _14=_11+_12;
}else{
var _14=_12;
var _15=this.pagerIconNext.height;
var _16=_8(_6,"height");
var _17=_16-(2*_15);
_8(_6,{height:_17+"px",marginTop:this.pagerIconNext.height+"px",marginBottom:this.pagerIconNext.height+"px"});
}
var _18=_8(this.pagerContainer,"width")-_14;
_8(_6,"width",_18+"px");
var _10=Math.floor(_8(_6,"height")/this.itemsPage);
if(this.statusPos=="trailing"){
if(this.pagerPos!="center"){
_8(_6,"marginLeft",_11+"px");
}
_8(_6,"marginRight",_12+"px");
}else{
_8(_6,"marginLeft",_12+"px");
if(this.pagerPos!="center"){
_8(_6,"marginRight",_11+"px");
}
}
}
var _19=_2.getObject(this.itemClass);
var _1a="padding"+(_7?"Left":"Top");
var _1b="padding"+(_7?"Right":"Bottom");
_2.forEach(this.items,function(_1c,cnt){
var _1d=_2.create("div",{innerHTML:_1c.content});
var _1e=new _19({id:this.id+"-item-"+(cnt+1)},_1d);
this.pagerItems.appendChild(_1e.domNode);
var _1f={};
_1f[(_7?"width":"height")]=(_10-this.itemSpace)+"px";
var p=(_7?"height":"width");
_1f[p]=_8(_6,p)+"px";
_8(_1e.containerNode,_1f);
if(this.resizeChildren){
_1e.resizeChildren();
}
_1e.parseChildren();
_8(_1e.domNode,"position","absolute");
if(cnt<this.itemsPage){
var pos=(cnt)*_10;
var _20=(_7?"left":"top");
var dir=(_7?"top":"left");
_8(_1e.domNode,dir,"0px");
_8(_1e.domNode,_20,pos+"px");
}else{
_8(_1e.domNode,"top","-1000px");
_8(_1e.domNode,"left","-1000px");
}
_8(_1e.domNode,_1b,(this.itemSpace/2)+"px");
_8(_1e.domNode,_1a,(this.itemSpace/2)+"px");
},this);
},_renderPager:function(){
var tcp=this.pagerContainerPager;
var _21="0px";
var _22=(this.orientation=="horizontal");
if(_22){
if(this.statusPos=="center"){
}else{
if(this.statusPos=="trailing"){
_2.style(tcp,"top",_21);
}else{
_2.style(tcp,"bottom",_21);
}
}
_2.style(this.pagerNext,"right",_21);
_2.style(this.pagerPrevious,"left",_21);
}else{
if(this.statusPos=="trailing"){
_2.style(tcp,"left",_21);
}else{
_2.style(tcp,"right",_21);
}
_2.style(this.pagerNext,"bottom",_21);
_2.style(this.pagerPrevious,"top",_21);
}
},_renderStatus:function(){
this._totalPages=Math.ceil(this.items.length/this.itemsPage);
this.iconWidth=0;
this.iconHeight=0;
this.iconsLoaded=0;
this._iconConnects=[];
for(var i=1;i<=this._totalPages;i++){
var _23=new Image();
var _24=i;
_2.connect(_23,"onclick",_2.hitch(this,function(_25){
this._pagerSkip(_25);
},_24));
this._iconConnects[_24]=_2.connect(_23,"onload",_2.hitch(this,function(_26){
this.iconWidth+=_23.width;
this.iconHeight+=_23.height;
this.iconsLoaded++;
if(this._totalPages==this.iconsLoaded){
if(this.orientation=="horizontal"){
if(this.statusPos=="trailing"){
if(this.pagerPos=="center"){
var _27=_2.style(this.pagerContainer,"height");
var _28=_2.style(this.pagerContainerStatus,"height");
_2.style(this.pagerContainerPager,"top",((_27/2)-(_28/2))+"px");
}
_2.style(this.pagerContainerStatus,"bottom","0px");
}else{
if(this.pagerPos=="center"){
var _27=_2.style(this.pagerContainer,"height");
var _28=_2.style(this.pagerContainerStatus,"height");
_2.style(this.pagerContainerPager,"bottom",((_27/2)-(_28/2))+"px");
}
_2.style(this.pagerContainerStatus,"top","0px");
}
var _29=(_2.style(this.pagerContainer,"width")/2)-(this.iconWidth/2);
_2.style(this.pagerContainerStatus,"paddingLeft",_29+"px");
}else{
if(this.statusPos=="trailing"){
if(this.pagerPos=="center"){
var _2a=_2.style(this.pagerContainer,"width");
var _2b=_2.style(this.pagerContainerStatus,"width");
_2.style(this.pagerContainerPager,"left",((_2a/2)-(_2b/2))+"px");
}
_2.style(this.pagerContainerStatus,"right","0px");
}else{
if(this.pagerPos=="center"){
var _2a=_2.style(this.pagerContainer,"width");
var _2b=_2.style(this.pagerContainerStatus,"width");
_2.style(this.pagerContainerPager,"right",((_2a/2)-(_2b/2))+"px");
}
_2.style(this.pagerContainerStatus,"left","0px");
}
var _29=(_2.style(this.pagerContainer,"height")/2)-(this.iconHeight/2);
_2.style(this.pagerContainerStatus,"paddingTop",_29+"px");
}
}
_2.disconnect(this._iconConnects[_26]);
},_24));
if(i==this._currentPage){
_23.src=this.iconPageActive;
}else{
_23.src=this.iconPage;
}
var _24=i;
_2.addClass(_23,this.orientation+"PagerIcon");
_2.attr(_23,"id",this.id+"-status-"+i);
this.pagerContainerStatus.appendChild(_23);
if(this.orientation=="vertical"){
_2.style(_23,"display","block");
}
}
},_pagerSkip:function(_2c){
if(this._currentPage==_2c){
return;
}else{
var _2d;
var _2e;
if(_2c<this._currentPage){
_2d=this._currentPage-_2c;
_2e=(this._totalPages+_2c)-this._currentPage;
}else{
_2d=(this._totalPages+this._currentPage)-_2c;
_2e=_2c-this._currentPage;
}
var b=(_2e>_2d);
this._toScroll=(b?_2d:_2e);
var cmd=(b?"_pagerPrevious":"_pagerNext");
var _2f=this.connect(this,"onScrollEnd",function(){
this._toScroll--;
if(this._toScroll<1){
this.disconnect(_2f);
}else{
this[cmd]();
}
});
this[cmd]();
}
},_pagerNext:function(){
if(this._anim){
return;
}
var _30=[];
for(var i=this._currentPage*this.itemsPage;i>(this._currentPage-1)*this.itemsPage;i--){
if(!_2.byId(this.id+"-item-"+i)){
continue;
}
var _31=_2.byId(this.id+"-item-"+i);
var _32=_2.marginBox(_31);
if(this.orientation=="horizontal"){
var _33=_32.l-(this.itemsPage*_32.w);
_30.push(_2.fx.slideTo({node:_31,left:_33,duration:this.duration}));
}else{
var _33=_32.t-(this.itemsPage*_32.h);
_30.push(_2.fx.slideTo({node:_31,top:_33,duration:this.duration}));
}
}
var _34=this._currentPage;
if(this._currentPage==this._totalPages){
this._currentPage=1;
}else{
this._currentPage++;
}
var cnt=this.itemsPage;
for(var i=this._currentPage*this.itemsPage;i>(this._currentPage-1)*this.itemsPage;i--){
if(_2.byId(this.id+"-item-"+i)){
var _31=_2.byId(this.id+"-item-"+i);
var _32=_2.marginBox(_31);
if(this.orientation=="horizontal"){
var _35=(_2.style(this.pagerContainerView,"width")+((cnt-1)*_32.w))-1;
_2.style(_31,"left",_35+"px");
_2.style(_31,"top","0px");
var _33=_35-(this.itemsPage*_32.w);
_30.push(_2.fx.slideTo({node:_31,left:_33,duration:this.duration}));
}else{
_35=(_2.style(this.pagerContainerView,"height")+((cnt-1)*_32.h))-1;
_2.style(_31,"top",_35+"px");
_2.style(_31,"left","0px");
var _33=_35-(this.itemsPage*_32.h);
_30.push(_2.fx.slideTo({node:_31,top:_33,duration:this.duration}));
}
}
cnt--;
}
this._anim=_2.fx.combine(_30);
var _36=this.connect(this._anim,"onEnd",function(){
delete this._anim;
this.onScrollEnd();
this.disconnect(_36);
});
this._anim.play();
_2.byId(this.id+"-status-"+_34).src=this.iconPage;
_2.byId(this.id+"-status-"+this._currentPage).src=this.iconPageActive;
},_pagerPrevious:function(){
if(this._anim){
return;
}
var _37=[];
for(var i=this._currentPage*this.itemsPage;i>(this._currentPage-1)*this.itemsPage;i--){
if(!_2.byId(this.id+"-item-"+i)){
continue;
}
var _38=_2.byId(this.id+"-item-"+i);
var _39=_2.marginBox(_38);
if(this.orientation=="horizontal"){
var _3a=_2.style(_38,"left")+(this.itemsPage*_39.w);
_37.push(_2.fx.slideTo({node:_38,left:_3a,duration:this.duration}));
}else{
var _3a=_2.style(_38,"top")+(this.itemsPage*_39.h);
_37.push(_2.fx.slideTo({node:_38,top:_3a,duration:this.duration}));
}
}
var _3b=this._currentPage;
if(this._currentPage==1){
this._currentPage=this._totalPages;
}else{
this._currentPage--;
}
var cnt=this.itemsPage;
var j=1;
for(var i=this._currentPage*this.itemsPage;i>(this._currentPage-1)*this.itemsPage;i--){
if(_2.byId(this.id+"-item-"+i)){
var _38=_2.byId(this.id+"-item-"+i);
var _39=_2.marginBox(_38);
if(this.orientation=="horizontal"){
var _3c=-(j*_39.w)+1;
_2.style(_38,"left",_3c+"px");
_2.style(_38,"top","0px");
var _3a=((cnt-1)*_39.w);
_37.push(_2.fx.slideTo({node:_38,left:_3a,duration:this.duration}));
var _3a=_3c+(this.itemsPage*_39.w);
_37.push(_2.fx.slideTo({node:_38,left:_3a,duration:this.duration}));
}else{
_3c=-((j*_39.h)+1);
_2.style(_38,"top",_3c+"px");
_2.style(_38,"left","0px");
var _3a=((cnt-1)*_39.h);
_37.push(_2.fx.slideTo({node:_38,top:_3a,duration:this.duration}));
}
}
cnt--;
j++;
}
this._anim=_2.fx.combine(_37);
var _3d=_2.connect(this._anim,"onEnd",_2.hitch(this,function(){
delete this._anim;
this.onScrollEnd();
_2.disconnect(_3d);
}));
this._anim.play();
_2.byId(this.id+"-status-"+_3b).src=this.iconPage;
_2.byId(this.id+"-status-"+this._currentPage).src=this.iconPageActive;
},onScrollEnd:function(){
}});
_2.declare("dojox.widget._PagerItem",[_1._Widget,_1._Templated],{templateString:"<li class=\"pagerItem\" dojoAttachPoint=\"containerNode\"></li>",resizeChildren:function(){
var box=_2.marginBox(this.containerNode);
_2.style(this.containerNode.firstChild,{width:box.w+"px",height:box.h+"px"});
},parseChildren:function(){
_2.parser.parse(this.containerNode);
}});
});
