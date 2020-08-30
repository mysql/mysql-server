//>>built
define("dojox/encoding/compression/lzw",["dojo/_base/lang","../bits"],function(_1,_2){
var _3=_1.getObject("dojox.encoding.compression.lzw",true);
var _4=function(x){
var w=1;
for(var v=2;x>=v;v<<=1,++w){
}
return w;
};
_3.Encoder=function(n){
this.size=n;
this.init();
};
_1.extend(_3.Encoder,{init:function(){
this.dict={};
for(var i=0;i<this.size;++i){
this.dict[String.fromCharCode(i)]=i;
}
this.width=_4(this.code=this.size);
this.p="";
},encode:function(_5,_6){
var c=String.fromCharCode(_5),p=this.p+c,r=0;
if(p in this.dict){
this.p=p;
return r;
}
_6.putBits(this.dict[this.p],this.width);
if((this.code&(this.code+1))==0){
_6.putBits(this.code++,r=this.width++);
}
this.dict[p]=this.code++;
this.p=c;
return r+this.width;
},flush:function(_7){
if(this.p.length==0){
return 0;
}
_7.putBits(this.dict[this.p],this.width);
this.p="";
return this.width;
}});
_3.Decoder=function(n){
this.size=n;
this.init();
};
_1.extend(_3.Decoder,{init:function(){
this.codes=new Array(this.size);
for(var i=0;i<this.size;++i){
this.codes[i]=String.fromCharCode(i);
}
this.width=_4(this.size);
this.p=-1;
},decode:function(_8){
var c=_8.getBits(this.width),v;
if(c<this.codes.length){
v=this.codes[c];
if(this.p>=0){
this.codes.push(this.codes[this.p]+v.substr(0,1));
}
}else{
if((c&(c+1))==0){
this.codes.push("");
++this.width;
return "";
}
var x=this.codes[this.p];
v=x+x.substr(0,1);
this.codes.push(v);
}
this.p=c;
return v;
}});
return _3;
});
