//>>built
define("dojox/fx/scroll",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/fx","dojox/fx/_base","dojox/fx/_core","dojo/dom-geometry","dojo/_base/sniff"],function(_1,_2,_3,_4,_5,_6,_7){
_1.experimental("dojox.fx.scroll");
var fx=_2.getObject("dojox.fx",true);
_4.smoothScroll=function(_8){
if(!_8.target){
_8.target=_6.position(_8.node);
}
var _9=_2[(_7("ie")?"isObject":"isFunction")](_8["win"].scrollTo),_a={x:_8.target.x,y:_8.target.y};
if(!_9){
var _b=_6.position(_8.win);
_a.x-=_b.x;
_a.y-=_b.y;
}
var _c=(_9)?(function(_d){
_8.win.scrollTo(_d[0],_d[1]);
}):(function(_e){
_8.win.scrollLeft=_e[0];
_8.win.scrollTop=_e[1];
});
var _f=new _3.Animation(_2.mixin({beforeBegin:function(){
if(this.curve){
delete this.curve;
}
var _10=_9?dojo._docScroll():{x:_8.win.scrollLeft,y:_8.win.scrollTop};
_f.curve=new _5([_10.x,_10.y],[_10.x+_a.x,_10.y+_a.y]);
},onAnimate:_c},_8));
return _f;
};
fx.smoothScroll=_4.smoothScroll;
return _4.smoothScroll;
});
