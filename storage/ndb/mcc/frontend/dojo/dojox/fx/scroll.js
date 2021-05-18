//>>built
define("dojox/fx/scroll",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/fx","dojo/_base/window","dojox/fx/_base","dojox/fx/_core","dojo/dom-geometry","dojo/_base/sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.fx.scroll");
var fx=_2.getObject("dojox.fx",true);
_5.smoothScroll=function(_9){
if(!_9.target){
_9.target=_7.position(_9.node);
}
var _a=_9["win"]===_4.global,_b={x:_9.target.x,y:_9.target.y};
if(!_a){
var _c=_7.position(_9.win);
_b.x-=_c.x;
_b.y-=_c.y;
}
var _d=(_a)?(function(_e){
_9.win.scrollTo(_e[0],_e[1]);
}):(function(_f){
_9.win.scrollLeft=_f[0];
_9.win.scrollTop=_f[1];
});
var _10=new _3.Animation(_2.mixin({beforeBegin:function(){
if(this.curve){
delete this.curve;
}
var _11=_a?dojo._docScroll():{x:_9.win.scrollLeft,y:_9.win.scrollTop};
_10.curve=new _6([_11.x,_11.y],[_11.x+_b.x,_11.y+_b.y]);
},onAnimate:_d},_9));
return _10;
};
fx.smoothScroll=_5.smoothScroll;
return _5.smoothScroll;
});
