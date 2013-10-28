//>>built
define("dojox/math/random/Secure",["dojo"],function(_1){
_1.declare("dojox.math.random.Secure",null,{constructor:function(_2,_3){
this.prng=_2;
var p=this.pool=new Array(_2.size);
this.pptr=0;
for(var i=0,_4=_2.size;i<_4;){
var t=Math.floor(65536*Math.random());
p[i++]=t>>>8;
p[i++]=t&255;
}
this.seedTime();
if(!_3){
this.h=[_1.connect(_1.body(),"onclick",this,"seedTime"),_1.connect(_1.body(),"onkeypress",this,"seedTime")];
}
},destroy:function(){
if(this.h){
_1.forEach(this.h,_1.disconnect);
}
},nextBytes:function(_5){
var _6=this.state;
if(!_6){
this.seedTime();
_6=this.state=this.prng();
_6.init(this.pool);
for(var p=this.pool,i=0,_7=p.length;i<_7;p[i++]=0){
}
this.pptr=0;
}
for(var i=0,_7=_5.length;i<_7;++i){
_5[i]=_6.next();
}
},seedTime:function(){
this._seed_int(new Date().getTime());
},_seed_int:function(x){
var p=this.pool,i=this.pptr;
p[i++]^=x&255;
p[i++]^=(x>>8)&255;
p[i++]^=(x>>16)&255;
p[i++]^=(x>>24)&255;
if(i>=this.prng.size){
i-=this.prng.size;
}
this.pptr=i;
}});
return dojox.math.random.Secure;
});
