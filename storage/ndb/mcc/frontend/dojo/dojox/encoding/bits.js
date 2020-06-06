//>>built
define("dojox/encoding/bits",["dojo/_base/lang"],function(_1){
var _2=_1.getObject("dojox.encoding.bits",true);
_2.OutputStream=function(){
this.reset();
};
_1.extend(_2.OutputStream,{reset:function(){
this.buffer=[];
this.accumulator=0;
this.available=8;
},putBits:function(_3,_4){
while(_4){
var w=Math.min(_4,this.available);
var v=(w<=_4?_3>>>(_4-w):_3)<<(this.available-w);
this.accumulator|=v&(255>>>(8-this.available));
this.available-=w;
if(!this.available){
this.buffer.push(this.accumulator);
this.accumulator=0;
this.available=8;
}
_4-=w;
}
},getWidth:function(){
return this.buffer.length*8+(8-this.available);
},getBuffer:function(){
var b=this.buffer;
if(this.available<8){
b.push(this.accumulator&(255<<this.available));
}
this.reset();
return b;
}});
_2.InputStream=function(_5,_6){
this.buffer=_5;
this.width=_6;
this.bbyte=this.bit=0;
};
_1.extend(_2.InputStream,{getBits:function(_7){
var r=0;
while(_7){
var w=Math.min(_7,8-this.bit);
var v=this.buffer[this.bbyte]>>>(8-this.bit-w);
r<<=w;
r|=v&~(~0<<w);
this.bit+=w;
if(this.bit==8){
++this.bbyte;
this.bit=0;
}
_7-=w;
}
return r;
},getWidth:function(){
return this.width-this.bbyte*8-this.bit;
}});
return _2;
});
