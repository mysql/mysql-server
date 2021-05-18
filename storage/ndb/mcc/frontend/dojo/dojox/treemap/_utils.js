//>>built
define("dojox/treemap/_utils",["dojo/_base/array"],function(_1){
var _2={group:function(_3,_4,_5){
var _6={children:[]};
var _7=function(_8,_9){
if(!_8.__treeValue){
_8.__treeValue=0;
}
_8.__treeValue+=_5(_9);
return _8;
};
_1.forEach(_3,function(_a){
var r=_6;
_1.forEach(_4,function(_b,j){
var _c=_b(_a);
var _d=_2.find(r.children,function(_e){
return (_e.__treeName==_c);
});
if(!_d){
r.children.push(_d={__treeName:_c,__treeID:_c+Math.random(),children:[]});
}
_d=_7(_d,_a);
if(j!=_4.length-1){
r=_d;
}else{
_d.children.push(_a);
}
});
r=_7(r,_a);
});
return _6;
},find:function(_f,_10){
var l=_f.length;
for(var i=0;i<l;++i){
if(_10.call(null,_f[i])){
return _f[i];
}
}
return null;
},solve:function(_11,_12,_13,_14,rtl){
var _15=_2.initElements(_11,_14);
var _16=_15.total;
var _17=_15.elements;
var _18=_16;
if(_16==0){
if(_17.length==0){
return {items:_11,rects:[],total:0};
}
_1.forEach(_17,function(_19){
_19.size=_19.sizeTmp=100;
});
_16=_17.length*100;
}
_17.sort(function(b,a){
return a.size-b.size;
});
_2._compute(_12,_13,_17,_16);
_17.sort(function(a,b){
return a.index-b.index;
});
var _1a={};
_1a.elements=_17;
_1a.size=_18;
rects=_1.map(_17,function(_1b){
return {x:rtl?_12-_1b.x-_1b.width:_1b.x,y:_1b.y,w:_1b.width,h:_1b.height};
});
_1a.rectangles=rects;
return _1a;
},initElements:function(_1c,_1d){
var _1e=0;
var _1f=_1.map(_1c,function(_20,_21){
var _22=_1d!=null?_1d(_20):0;
if(_22<0){
throw new Error("item size dimension must be positive");
}
_1e+=_22;
return {index:_21,size:_22,sizeTmp:_22};
});
return {elements:_1f,total:_1e};
},_compute:function(_23,_24,_25,_26){
var _27=((_23*_24)/_26)/100;
_1.forEach(_25,function(_28){
_28.sizeTmp*=_27;
});
var _29=0;
var end=0;
var _2a=-1>>>1;
var _2b;
var _2c=0;
var _2d=0;
var _2e=_23;
var _2f=_24;
var _30=_2e>_2f;
while(end!=_25.length){
_2b=_2._trySolution(_25,_29,end,_30,_2e,_2f);
if((_2b>_2a)||(_2b<1)){
var _31=0;
var _32=0;
for(var n=_29;n<end;n++){
_25[n].x=_2c+_31;
_25[n].y=_2d+_32;
if(_30){
_32+=_25[n].height;
}else{
_31+=_25[n].width;
}
}
if(_30){
_2c+=_25[_29].width;
}else{
_2d+=_25[_29].height;
}
_2e=_23-_2c;
_2f=_24-_2d;
_30=_2e>_2f;
_29=end;
end=_29;
_2a=-1>>>1;
continue;
}else{
for(var n=_29;n<=end;n++){
_25[n].width=_25[n].widthTmp;
_25[n].height=_25[n].heightTmp;
}
_2a=_2b;
}
end++;
}
var _33=0;
var _34=0;
for(var n=_29;n<end;n++){
_25[n].x=_2c+_33;
_25[n].y=_2d+_34;
if(_30){
_34+=_25[n].height;
}else{
_33+=_25[n].width;
}
}
},_trySolution:function(_35,_36,end,_37,_38,_39){
var _3a=0;
var _3b=0;
var _3c=0;
var _3d=0;
for(var n=_36;n<=end;n++){
_3a+=_35[n].sizeTmp;
}
if(_37){
if(_39==0){
_3c=_3d=0;
}else{
_3c=_3a/_39*100;
_3d=_39;
}
}else{
if(_38==0){
_3c=_3d=0;
}else{
_3d=_3a/_38*100;
_3c=_38;
}
}
for(var n=_36;n<=end;n++){
if(_37){
_35[n].widthTmp=_3c;
if(_3a==0){
_35[n].heightTmp=0;
}else{
_35[n].heightTmp=_3d*_35[n].sizeTmp/_3a;
}
}else{
if(_3a==0){
_35[n].widthTmp=0;
}else{
_35[n].widthTmp=_3c*_35[n].sizeTmp/_3a;
}
_35[n].heightTmp=_3d;
}
}
_3b=Math.max(_35[end].heightTmp/_35[end].widthTmp,_35[end].widthTmp/_35[end].heightTmp);
if(_3b==undefined){
return 1;
}
return _3b;
}};
return _2;
});
