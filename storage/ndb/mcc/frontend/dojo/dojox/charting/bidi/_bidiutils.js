//>>built
define("dojox/charting/bidi/_bidiutils",{reverseMatrix:function(_1,_2,_3,_4){
var _5=_3.l-_3.r;
var xx=_4?-1:1;
var xy=0;
var yx=0;
var yy=1;
var dx=_4?_2.width+_5:0;
var dy=0;
if(_1.matrix){
xx=xx*Math.abs(_1.matrix.xx);
yy=_1.matrix.yy;
xy=_1.matrix.xy;
yx=_1.matrix.yx;
dy=_1.matrix.xy;
}
_1.setTransform({xx:xx,xy:xy,yx:yx,yy:yy,dx:dx,dy:dy});
}});
