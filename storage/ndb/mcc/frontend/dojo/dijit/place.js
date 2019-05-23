//>>built
define("dijit/place",["dojo/_base/array","dojo/dom-geometry","dojo/dom-style","dojo/_base/kernel","dojo/_base/window","./Viewport","./main"],function(_1,_2,_3,_4,_5,_6,_7){
function _8(_9,_a,_b,_c){
var _d=_6.getEffectiveBox(_9.ownerDocument);
if(!_9.parentNode||String(_9.parentNode.tagName).toLowerCase()!="body"){
_5.body(_9.ownerDocument).appendChild(_9);
}
var _e=null;
_1.some(_a,function(_f){
var _10=_f.corner;
var pos=_f.pos;
var _11=0;
var _12={w:{"L":_d.l+_d.w-pos.x,"R":pos.x-_d.l,"M":_d.w}[_10.charAt(1)],h:{"T":_d.t+_d.h-pos.y,"B":pos.y-_d.t,"M":_d.h}[_10.charAt(0)]};
var s=_9.style;
s.left=s.right="auto";
if(_b){
var res=_b(_9,_f.aroundCorner,_10,_12,_c);
_11=typeof res=="undefined"?0:res;
}
var _13=_9.style;
var _14=_13.display;
var _15=_13.visibility;
if(_13.display=="none"){
_13.visibility="hidden";
_13.display="";
}
var bb=_2.position(_9);
_13.display=_14;
_13.visibility=_15;
var _16={"L":pos.x,"R":pos.x-bb.w,"M":Math.max(_d.l,Math.min(_d.l+_d.w,pos.x+(bb.w>>1))-bb.w)}[_10.charAt(1)],_17={"T":pos.y,"B":pos.y-bb.h,"M":Math.max(_d.t,Math.min(_d.t+_d.h,pos.y+(bb.h>>1))-bb.h)}[_10.charAt(0)],_18=Math.max(_d.l,_16),_19=Math.max(_d.t,_17),_1a=Math.min(_d.l+_d.w,_16+bb.w),_1b=Math.min(_d.t+_d.h,_17+bb.h),_1c=_1a-_18,_1d=_1b-_19;
_11+=(bb.w-_1c)+(bb.h-_1d);
if(_e==null||_11<_e.overflow){
_e={corner:_10,aroundCorner:_f.aroundCorner,x:_18,y:_19,w:_1c,h:_1d,overflow:_11,spaceAvailable:_12};
}
return !_11;
});
if(_e.overflow&&_b){
_b(_9,_e.aroundCorner,_e.corner,_e.spaceAvailable,_c);
}
var s=_9.style;
s.top=_e.y+"px";
s.left=_e.x+"px";
s.right="auto";
return _e;
};
var _1e={at:function(_1f,pos,_20,_21){
var _22=_1.map(_20,function(_23){
var c={corner:_23,pos:{x:pos.x,y:pos.y}};
if(_21){
c.pos.x+=_23.charAt(1)=="L"?_21.x:-_21.x;
c.pos.y+=_23.charAt(0)=="T"?_21.y:-_21.y;
}
return c;
});
return _8(_1f,_22);
},around:function(_24,_25,_26,_27,_28){
var _29=(typeof _25=="string"||"offsetWidth" in _25||"ownerSVGElement" in _25)?_2.position(_25,true):_25;
if(_25.parentNode){
var _2a=_3.getComputedStyle(_25).position=="absolute";
var _2b=_25.parentNode;
while(_2b&&_2b.nodeType==1&&_2b.nodeName!="BODY"){
var _2c=_2.position(_2b,true),pcs=_3.getComputedStyle(_2b);
if(/relative|absolute/.test(pcs.position)){
_2a=false;
}
if(!_2a&&/hidden|auto|scroll/.test(pcs.overflow)){
var _2d=Math.min(_29.y+_29.h,_2c.y+_2c.h);
var _2e=Math.min(_29.x+_29.w,_2c.x+_2c.w);
_29.x=Math.max(_29.x,_2c.x);
_29.y=Math.max(_29.y,_2c.y);
_29.h=_2d-_29.y;
_29.w=_2e-_29.x;
}
if(pcs.position=="absolute"){
_2a=true;
}
_2b=_2b.parentNode;
}
}
var x=_29.x,y=_29.y,_2f="w" in _29?_29.w:(_29.w=_29.width),_30="h" in _29?_29.h:(_4.deprecated("place.around: dijit/place.__Rectangle: { x:"+x+", y:"+y+", height:"+_29.height+", width:"+_2f+" } has been deprecated.  Please use { x:"+x+", y:"+y+", h:"+_29.height+", w:"+_2f+" }","","2.0"),_29.h=_29.height);
var _31=[];
function _32(_33,_34){
_31.push({aroundCorner:_33,corner:_34,pos:{x:{"L":x,"R":x+_2f,"M":x+(_2f>>1)}[_33.charAt(1)],y:{"T":y,"B":y+_30,"M":y+(_30>>1)}[_33.charAt(0)]}});
};
_1.forEach(_26,function(pos){
var ltr=_27;
switch(pos){
case "above-centered":
_32("TM","BM");
break;
case "below-centered":
_32("BM","TM");
break;
case "after-centered":
ltr=!ltr;
case "before-centered":
_32(ltr?"ML":"MR",ltr?"MR":"ML");
break;
case "after":
ltr=!ltr;
case "before":
_32(ltr?"TL":"TR",ltr?"TR":"TL");
_32(ltr?"BL":"BR",ltr?"BR":"BL");
break;
case "below-alt":
ltr=!ltr;
case "below":
_32(ltr?"BL":"BR",ltr?"TL":"TR");
_32(ltr?"BR":"BL",ltr?"TR":"TL");
break;
case "above-alt":
ltr=!ltr;
case "above":
_32(ltr?"TL":"TR",ltr?"BL":"BR");
_32(ltr?"TR":"TL",ltr?"BR":"BL");
break;
default:
_32(pos.aroundCorner,pos.corner);
}
});
var _35=_8(_24,_31,_28,{w:_2f,h:_30});
_35.aroundNodePos=_29;
return _35;
}};
return _7.place=_1e;
});
