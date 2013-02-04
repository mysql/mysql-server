//>>built
define("dojox/fx/flip",["dojo/_base/kernel","dojo/_base/html","dojo/dom","dojo/dom-construct","dojo/dom-geometry","dojo/_base/connect","dojo/_base/Color","dojo/_base/sniff","dojo/_base/lang","dojo/_base/window","dojo/_base/fx","dojo/fx","./_base"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
_1.experimental("dojox.fx.flip");
var _e="border",_f="Width",_10="Height",_11="Top",_12="Right",_13="Left",_14="Bottom";
_d.flip=function(_15){
var _16=_4.create("div"),_17=_15.node=_3.byId(_15.node),s=_17.style,_18=null,hs=null,pn=null,_19=_15.lightColor||"#dddddd",_1a=_15.darkColor||"#555555",_1b=_2.style(_17,"backgroundColor"),_1c=_15.endColor||_1b,_1d={},_1e=[],_1f=_15.duration?_15.duration/2:250,dir=_15.dir||"left",_20=0.9,_21="transparent",_22=_15.whichAnim,_23=_15.axis||"center",_24=_15.depth;
var _25=function(_26){
return ((new _7(_26)).toHex()==="#000000")?"#000001":_26;
};
if(_8("ie")<7){
_1c=_25(_1c);
_19=_25(_19);
_1a=_25(_1a);
_1b=_25(_1b);
_21="black";
_16.style.filter="chroma(color='#000000')";
}
var _27=(function(n){
return function(){
var ret=_2.coords(n,true);
_18={top:ret.y,left:ret.x,width:ret.w,height:ret.h};
};
})(_17);
_27();
hs={position:"absolute",top:_18["top"]+"px",left:_18["left"]+"px",height:"0",width:"0",zIndex:_15.zIndex||(s.zIndex||0),border:"0 solid "+_21,fontSize:"0",visibility:"hidden"};
var _28=[{},{top:_18["top"],left:_18["left"]}];
var _29={left:[_13,_12,_11,_14,_f,_10,"end"+_10+"Min",_13,"end"+_10+"Max"],right:[_12,_13,_11,_14,_f,_10,"end"+_10+"Min",_13,"end"+_10+"Max"],top:[_11,_14,_13,_12,_10,_f,"end"+_f+"Min",_11,"end"+_f+"Max"],bottom:[_14,_11,_13,_12,_10,_f,"end"+_f+"Min",_11,"end"+_f+"Max"]};
pn=_29[dir];
if(typeof _24!="undefined"){
_24=Math.max(0,Math.min(1,_24))/2;
_20=0.4+(0.5-_24);
}else{
_20=Math.min(0.9,Math.max(0.4,_18[pn[5].toLowerCase()]/_18[pn[4].toLowerCase()]));
}
var p0=_28[0];
for(var i=4;i<6;i++){
if(_23=="center"||_23=="cube"){
_18["end"+pn[i]+"Min"]=_18[pn[i].toLowerCase()]*_20;
_18["end"+pn[i]+"Max"]=_18[pn[i].toLowerCase()]/_20;
}else{
if(_23=="shortside"){
_18["end"+pn[i]+"Min"]=_18[pn[i].toLowerCase()];
_18["end"+pn[i]+"Max"]=_18[pn[i].toLowerCase()]/_20;
}else{
if(_23=="longside"){
_18["end"+pn[i]+"Min"]=_18[pn[i].toLowerCase()]*_20;
_18["end"+pn[i]+"Max"]=_18[pn[i].toLowerCase()];
}
}
}
}
if(_23=="center"){
p0[pn[2].toLowerCase()]=_18[pn[2].toLowerCase()]-(_18[pn[8]]-_18[pn[6]])/4;
}else{
if(_23=="shortside"){
p0[pn[2].toLowerCase()]=_18[pn[2].toLowerCase()]-(_18[pn[8]]-_18[pn[6]])/2;
}
}
_1d[pn[5].toLowerCase()]=_18[pn[5].toLowerCase()]+"px";
_1d[pn[4].toLowerCase()]="0";
_1d[_e+pn[1]+_f]=_18[pn[4].toLowerCase()]+"px";
_1d[_e+pn[1]+"Color"]=_1b;
p0[_e+pn[1]+_f]=0;
p0[_e+pn[1]+"Color"]=_1a;
p0[_e+pn[2]+_f]=p0[_e+pn[3]+_f]=_23!="cube"?(_18["end"+pn[5]+"Max"]-_18["end"+pn[5]+"Min"])/2:_18[pn[6]]/2;
p0[pn[7].toLowerCase()]=_18[pn[7].toLowerCase()]+_18[pn[4].toLowerCase()]/2+(_15.shift||0);
p0[pn[5].toLowerCase()]=_18[pn[6]];
var p1=_28[1];
p1[_e+pn[0]+"Color"]={start:_19,end:_1c};
p1[_e+pn[0]+_f]=_18[pn[4].toLowerCase()];
p1[_e+pn[2]+_f]=0;
p1[_e+pn[3]+_f]=0;
p1[pn[5].toLowerCase()]={start:_18[pn[6]],end:_18[pn[5].toLowerCase()]};
_9.mixin(hs,_1d);
_2.style(_16,hs);
_a.body().appendChild(_16);
var _2a=function(){
_4.destroy(_16);
s.backgroundColor=_1c;
s.visibility="visible";
};
if(_22=="last"){
for(i in p0){
p0[i]={start:p0[i]};
}
p0[_e+pn[1]+"Color"]={start:_1a,end:_1c};
p1=p0;
}
if(!_22||_22=="first"){
_1e.push(_b.animateProperty({node:_16,duration:_1f,properties:p0}));
}
if(!_22||_22=="last"){
_1e.push(_b.animateProperty({node:_16,duration:_1f,properties:p1,onEnd:_2a}));
}
_6.connect(_1e[0],"play",function(){
_16.style.visibility="visible";
s.visibility="hidden";
});
return _c.chain(_1e);
};
_d.flipCube=function(_2b){
var _2c=[],mb=_5.getMarginBox(_2b.node),_2d=mb.w/2,_2e=mb.h/2,_2f={top:{pName:"height",args:[{whichAnim:"first",dir:"top",shift:-_2e},{whichAnim:"last",dir:"bottom",shift:_2e}]},right:{pName:"width",args:[{whichAnim:"first",dir:"right",shift:_2d},{whichAnim:"last",dir:"left",shift:-_2d}]},bottom:{pName:"height",args:[{whichAnim:"first",dir:"bottom",shift:_2e},{whichAnim:"last",dir:"top",shift:-_2e}]},left:{pName:"width",args:[{whichAnim:"first",dir:"left",shift:-_2d},{whichAnim:"last",dir:"right",shift:_2d}]}};
var d=_2f[_2b.dir||"left"],p=d.args;
_2b.duration=_2b.duration?_2b.duration*2:500;
_2b.depth=0.8;
_2b.axis="cube";
for(var i=p.length-1;i>=0;i--){
_9.mixin(_2b,p[i]);
_2c.push(_d.flip(_2b));
}
return _c.combine(_2c);
};
_d.flipPage=function(_30){
var n=_30.node,_31=_2.coords(n,true),x=_31.x,y=_31.y,w=_31.w,h=_31.h,_32=_2.style(n,"backgroundColor"),_33=_30.lightColor||"#dddddd",_34=_30.darkColor,_35=_4.create("div"),_36=[],hn=[],dir=_30.dir||"right",pn={left:["left","right","x","w"],top:["top","bottom","y","h"],right:["left","left","x","w"],bottom:["top","top","y","h"]},_37={right:[1,-1],left:[-1,1],top:[-1,1],bottom:[1,-1]};
_2.style(_35,{position:"absolute",width:w+"px",height:h+"px",top:y+"px",left:x+"px",visibility:"hidden"});
var hs=[];
for(var i=0;i<2;i++){
var r=i%2,d=r?pn[dir][1]:dir,wa=r?"last":"first",_38=r?_32:_33,_39=r?_38:_30.startColor||n.style.backgroundColor;
hn[i]=_9.clone(_35);
var _3a=function(x){
return function(){
_4.destroy(hn[x]);
};
}(i);
_a.body().appendChild(hn[i]);
hs[i]={backgroundColor:r?_39:_32};
hs[i][pn[dir][0]]=_31[pn[dir][2]]+_37[dir][0]*i*_31[pn[dir][3]]+"px";
_2.style(hn[i],hs[i]);
_36.push(dojox.fx.flip({node:hn[i],dir:d,axis:"shortside",depth:_30.depth,duration:_30.duration/2,shift:_37[dir][i]*_31[pn[dir][3]]/2,darkColor:_34,lightColor:_33,whichAnim:wa,endColor:_38}));
_6.connect(_36[i],"onEnd",_3a);
}
return _c.chain(_36);
};
_d.flipGrid=function(_3b){
var _3c=_3b.rows||4,_3d=_3b.cols||4,_3e=[],_3f=_4.create("div"),n=_3b.node,_40=_2.coords(n,true),x=_40.x,y=_40.y,nw=_40.w,nh=_40.h,w=_40.w/_3d,h=_40.h/_3c,_41=[];
_2.style(_3f,{position:"absolute",width:w+"px",height:h+"px",backgroundColor:_2.style(n,"backgroundColor")});
for(var i=0;i<_3c;i++){
var r=i%2,d=r?"right":"left",_42=r?1:-1;
var cn=_9.clone(n);
_2.style(cn,{position:"absolute",width:nw+"px",height:nh+"px",top:y+"px",left:x+"px",clip:"rect("+i*h+"px,"+nw+"px,"+nh+"px,0)"});
_a.body().appendChild(cn);
_3e[i]=[];
for(var j=0;j<_3d;j++){
var hn=_9.clone(_3f),l=r?j:_3d-(j+1);
var _43=function(xn,_44,_45){
return function(){
if(!(_44%2)){
_2.style(xn,{clip:"rect("+_44*h+"px,"+(nw-(_45+1)*w)+"px,"+((_44+1)*h)+"px,0px)"});
}else{
_2.style(xn,{clip:"rect("+_44*h+"px,"+nw+"px,"+((_44+1)*h)+"px,"+((_45+1)*w)+"px)"});
}
};
}(cn,i,j);
_a.body().appendChild(hn);
_2.style(hn,{left:x+l*w+"px",top:y+i*h+"px",visibility:"hidden"});
var a=dojox.fx.flipPage({node:hn,dir:d,duration:_3b.duration||900,shift:_42*w/2,depth:0.2,darkColor:_3b.darkColor,lightColor:_3b.lightColor,startColor:_3b.startColor||_3b.node.style.backgroundColor}),_46=function(xn){
return function(){
_4.destroy(xn);
};
}(hn);
_6.connect(a,"play",this,_43);
_6.connect(a,"play",this,_46);
_3e[i].push(a);
}
_41.push(_c.chain(_3e[i]));
}
_6.connect(_41[0],"play",function(){
_2.style(n,{visibility:"hidden"});
});
return _c.combine(_41);
};
return _d;
});
