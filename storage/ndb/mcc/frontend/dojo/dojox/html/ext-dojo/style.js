//>>built
define("dojox/html/ext-dojo/style",["dojo/_base/kernel","dojo/dom-style","dojo/_base/lang","dojo/_base/html","dojo/_base/sniff","dojo/_base/window","dojo/dom","dojo/dom-construct","dojo/dom-style","dojo/dom-attr"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
_1.experimental("dojox.html.ext-dojo.style");
var st=_3.getObject("dojox.html.ext-dojo.style",true);
var _b=_3.getObject("dojox.html");
_3.mixin(_b["ext-dojo"].style,{supportsTransform:true,_toPx:function(_c){
var ds=_4.style,_d=this._conversion;
if(typeof _c==="number"){
return _c+"px";
}else{
if(_c.toLowerCase().indexOf("px")!=-1){
return _c;
}
}
!_d.parentNode&&_8.place(_d,_6.body());
ds(_d,"margin",_c);
return ds(_d,"margin");
},init:function(){
var _e=_6.doc.documentElement.style,_f=_b["ext-dojo"].style,_10=_9.get,_11=_9.set;
_9.get=function(_12,_13){
var tr=(_13=="transform"),to=(_13=="transformOrigin");
if(tr){
return _f.getTransform(_12);
}else{
if(to){
return _f.getTransformOrigin(_12);
}else{
return _10.apply(this,arguments);
}
}
};
_9.set=function(_14,_15,_16){
var tr=(_15=="transform"),to=(_15=="transformOrigin"),n=_7.byId(_14);
if(tr){
return _f.setTransform(n,_16,true);
}else{
if(to){
return _f.setTransformOrigin(n,_16);
}else{
return _11.apply(this,arguments);
}
}
};
for(var i=0,_17=["WebkitT","MozT","OT","msT","t"];i<_17.length;i++){
if(typeof _e[_17[i]+"ransform"]!=="undefined"){
this.tPropertyName=_17[i]+"ransform";
}
if(typeof _e[_17[i]+"ransformOrigin"]!=="undefined"){
this.toPropertyName=_17[i]+"ransformOrigin";
}
}
if(this.tPropertyName){
this.setTransform=function(_18,_19){
return _11(_18,this.tPropertyName,_19);
};
this.getTransform=function(_1a){
return _10(_1a,this.tPropertyName);
};
}else{
if(_5("ie")){
this.setTransform=this._setTransformFilter;
this.getTransform=this._getTransformFilter;
}
}
if(this.toPropertyName){
this.setTransformOrigin=function(_1b,_1c){
return _11(_1b,this.toPropertyName,_1c);
};
this.getTransformOrigin=function(_1d){
return _10(_1d,this.toPropertyName);
};
}else{
if(_5("ie")){
this.setTransformOrigin=this._setTransformOriginFilter;
this.getTransformOrigin=this._getTransformOriginFilter;
}else{
this.supportsTransform=false;
}
}
this._conversion=_8.create("div",{style:{position:"absolute",top:"-100px",left:"-100px",fontSize:0,width:"0",backgroundPosition:"50% 50%"}});
},_notSupported:function(){
console.warn("Sorry, this browser doesn't support transform and transform-origin");
},_setTransformOriginFilter:function(_1e,_1f){
var to=_3.trim(_1f).replace(" top"," 0").replace("left ","0 ").replace(" center","50%").replace("center ","50% ").replace(" bottom"," 100%").replace("right ","100% ").replace(/\s+/," "),_20=to.split(" "),n=_7.byId(_1e),t=this.getTransform(n),_21=true;
for(var i=0;i<_20.length;i++){
_21=_21&&/^0|(\d+(%|px|pt|in|pc|mm|cm))$/.test(_20[i]);
if(_20[i].indexOf("%")==-1){
_20[i]=this._toPx(_20[i]);
}
}
if(!_21||!_20.length||_20.length>2){
return _1f;
}
_4.attr(n,"dojo-transform-origin",_20.join(" "));
t&&this.setTransform(_1e,t);
return _1f;
},_getTransformOriginFilter:function(_22){
return _4.attr(_22,"dojo-transform-origin")||"50% 50%";
},_setTransformFilter:function(_23,_24){
var t=_24.replace(/\s/g,""),n=_7.byId(_23),_25=t.split(")"),_26=1,_27=1,_28="DXImageTransform.Microsoft.Matrix",_29=_a.has,_2a=_4.attr,PI=Math.PI,cos=Math.cos,sin=Math.sin,tan=Math.tan,max=Math.max,min=Math.min,abs=Math.abs,_2b=PI/180,_2c=PI/200,ct="",_2d="",_2e=[],x0=0,y0=0,dx=0,dy=0,xc=0,yc=0,a=0,m11=1,m12=0,m21=0,m22=1,tx=0,ty=0,_2f=[m11,m12,m21,m22,tx,ty],_30=false,ds=_4.style,_31=ds(n,"position")=="absolute"?"absolute":"relative",w=ds(n,"width")+ds(n,"paddingLeft")+ds(n,"paddingRight"),h=ds(n,"height")+ds(n,"paddingTop")+ds(n,"paddingBottom"),_32=this._toPx;
!_29(n,"dojo-transform-origin")&&this.setTransformOrigin(n,"50% 50%");
for(var i=0,l=_25.length;i<l;i++){
_2e=_25[i].match(/matrix|rotate|scaleX|scaleY|scale|skewX|skewY|skew|translateX|translateY|translate/);
_2d=_2e?_2e[0]:"";
switch(_2d){
case "matrix":
ct=_25[i].replace(/matrix\(|\)/g,"");
var _33=ct.split(",");
m11=_2f[0]*_33[0]+_2f[1]*_33[2];
m12=_2f[0]*_33[1]+_2f[1]*_33[3];
m21=_2f[2]*_33[0]+_2f[3]*_33[2];
m22=_2f[2]*_33[1]+_2f[3]*_33[3];
tx=_2f[4]+_33[4];
ty=_2f[5]+_33[5];
break;
case "rotate":
ct=_25[i].replace(/rotate\(|\)/g,"");
_26=ct.indexOf("deg")!=-1?_2b:ct.indexOf("grad")!=-1?_2c:1;
a=parseFloat(ct)*_26;
var s=sin(a),c=cos(a);
m11=_2f[0]*c+_2f[1]*s;
m12=-_2f[0]*s+_2f[1]*c;
m21=_2f[2]*c+_2f[3]*s;
m22=-_2f[2]*s+_2f[3]*c;
break;
case "skewX":
ct=_25[i].replace(/skewX\(|\)/g,"");
_26=ct.indexOf("deg")!=-1?_2b:ct.indexOf("grad")!=-1?_2c:1;
var ta=tan(parseFloat(ct)*_26);
m11=_2f[0];
m12=_2f[0]*ta+_2f[1];
m21=_2f[2];
m22=_2f[2]*ta+_2f[3];
break;
case "skewY":
ct=_25[i].replace(/skewY\(|\)/g,"");
_26=ct.indexOf("deg")!=-1?_2b:ct.indexOf("grad")!=-1?_2c:1;
ta=tan(parseFloat(ct)*_26);
m11=_2f[0]+_2f[1]*ta;
m12=_2f[1];
m21=_2f[2]+_2f[3]*ta;
m22=_2f[3];
break;
case "skew":
ct=_25[i].replace(/skew\(|\)/g,"");
var _34=ct.split(",");
_34[1]=_34[1]||"0";
_26=_34[0].indexOf("deg")!=-1?_2b:_34[0].indexOf("grad")!=-1?_2c:1;
_27=_34[1].indexOf("deg")!=-1?_2b:_34[1].indexOf("grad")!=-1?_2c:1;
var a0=tan(parseFloat(_34[0])*_26),a1=tan(parseFloat(_34[1])*_27);
m11=_2f[0]+_2f[1]*a1;
m12=_2f[0]*a0+_2f[1];
m21=_2f[2]+_2f[3]*a1;
m22=_2f[2]*a0+_2f[3];
break;
case "scaleX":
ct=parseFloat(_25[i].replace(/scaleX\(|\)/g,""))||1;
m11=_2f[0]*ct;
m12=_2f[1];
m21=_2f[2]*ct;
m22=_2f[3];
break;
case "scaleY":
ct=parseFloat(_25[i].replace(/scaleY\(|\)/g,""))||1;
m11=_2f[0];
m12=_2f[1]*ct;
m21=_2f[2];
m22=_2f[3]*ct;
break;
case "scale":
ct=_25[i].replace(/scale\(|\)/g,"");
var _35=ct.split(",");
_35[1]=_35[1]||_35[0];
m11=_2f[0]*_35[0];
m12=_2f[1]*_35[1];
m21=_2f[2]*_35[0];
m22=_2f[3]*_35[1];
break;
case "translateX":
ct=parseInt(_25[i].replace(/translateX\(|\)/g,""))||1;
m11=_2f[0];
m12=_2f[1];
m21=_2f[2];
m22=_2f[3];
tx=_32(ct);
tx&&_2a(n,"dojo-transform-matrix-tx",tx);
break;
case "translateY":
ct=parseInt(_25[i].replace(/translateY\(|\)/g,""))||1;
m11=_2f[0];
m12=_2f[1];
m21=_2f[2];
m22=_2f[3];
ty=_32(ct);
ty&&_2a(n,"dojo-transform-matrix-ty",ty);
break;
case "translate":
ct=_25[i].replace(/translate\(|\)/g,"");
m11=_2f[0];
m12=_2f[1];
m21=_2f[2];
m22=_2f[3];
var _36=ct.split(",");
_36[0]=parseInt(_32(_36[0]))||0;
_36[1]=parseInt(_32(_36[1]))||0;
tx=_36[0];
ty=_36[1];
tx&&_2a(n,"dojo-transform-matrix-tx",tx);
ty&&_2a(n,"dojo-transform-matrix-ty",ty);
break;
}
_2f=[m11,m12,m21,m22,tx,ty];
}
var Bx=min(w*m11+h*m12,min(min(w*m11,h*m12),0)),By=min(w*m21+h*m22,min(min(w*m21,h*m22),0));
dx=-Bx;
dy=-By;
if(_5("ie")<8){
n.style.zoom="1";
if(_31!="absolute"){
var _37=ds(_23.parentNode,"width"),tw=abs(w*m11),th=abs(h*m12),_38=max(tw+th,max(max(th,tw),0));
dx-=(_38-w)/2-(_37>_38?0:(_38-_37)/2);
}
}else{
if(_5("ie")==8){
ds(n,"zIndex")=="auto"&&(n.style.zIndex="0");
}
}
try{
_30=!!n.filters.item(_28);
}
catch(e){
_30=false;
}
if(_30){
n.filters.item(_28).M11=m11;
n.filters.item(_28).M12=m12;
n.filters.item(_28).M21=m21;
n.filters.item(_28).M22=m22;
n.filters.item(_28).filterType="bilinear";
n.filters.item(_28).Dx=0;
n.filters.item(_28).Dy=0;
n.filters.item(_28).sizingMethod="auto expand";
}else{
n.style.filter+=" progid:"+_28+"(M11="+m11+",M12="+m12+",M21="+m21+",M22="+m22+",FilterType='bilinear',Dx=0,Dy=0,sizingMethod='auto expand')";
}
tx=parseInt(_2a(n,"dojo-transform-matrix-tx")||"0");
ty=parseInt(_2a(n,"dojo-transform-matrix-ty")||"0");
var _39=_2a(n,"dojo-transform-origin").split(" ");
for(i=0;i<2;i++){
_39[i]=_39[i]||"50%";
}
xc=(_39[0].toString().indexOf("%")!=-1)?w*parseInt(_39[0])*0.01:_39[0];
yc=(_39[1].toString().indexOf("%")!=-1)?h*parseInt(_39[1])*0.01:_39[1];
if(_29(n,"dojo-startX")){
x0=parseInt(_2a(n,"dojo-startX"));
}else{
x0=parseInt(ds(n,"left"));
_2a(n,"dojo-startX",_31=="absolute"?x0:"0");
}
if(_29(n,"dojo-startY")){
y0=parseInt(_2a(n,"dojo-startY"));
}else{
y0=parseInt(ds(n,"top"));
_2a(n,"dojo-startY",_31=="absolute"?y0:"0");
}
ds(n,{position:_31,left:x0-parseInt(dx)+parseInt(xc)-((parseInt(xc)-tx)*m11+(parseInt(yc)-ty)*m12)+"px",top:y0-parseInt(dy)+parseInt(yc)-((parseInt(xc)-tx)*m21+(parseInt(yc)-ty)*m22)+"px"});
return _24;
},_getTransformFilter:function(_3a){
try{
var n=_7.byId(_3a),_3b=n.filters.item(0);
return "matrix("+_3b.M11+", "+_3b.M12+", "+_3b.M21+", "+_3b.M22+", "+(_4.attr(_3a,"dojo-transform-tx")||"0")+", "+(_4.attr(_3a,"dojo-transform-ty")||"0")+")";
}
catch(e){
return "matrix(1, 0, 0, 1, 0, 0)";
}
},setTransform:function(){
this._notSupported();
},setTransformOrigin:function(){
this._notSupported();
}});
_b["ext-dojo"].style.init();
return _4.style;
});
