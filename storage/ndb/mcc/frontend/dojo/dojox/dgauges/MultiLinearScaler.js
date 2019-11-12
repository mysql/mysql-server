//>>built
define("dojox/dgauges/MultiLinearScaler",["dojo/_base/declare","dojo/Stateful"],function(_1,_2){
return _1("dojox.dgauges.MultiLinearScaler",_2,{majorTickValues:null,minorTickCount:4,majorTicks:null,minorTicks:null,_snapIntervalPrecision:null,_snapCount:4,_snapIntervalPrecision:6,constructor:function(){
this.watchedProperties=["majorTickValues","snapCount","minorTickCount"];
},computeTicks:function(){
this.majorTicks=[];
this.minorTicks=[];
var ti;
var _3=1/(this.majorTickValues.length-1);
var _4=_3/(this.minorTickCount+1);
var _5=0;
var _6;
var _7;
var v;
for(var i=0;i<this.majorTickValues.length;i++){
v=this.majorTickValues[i];
ti={scaler:this};
ti.position=_5*_3;
ti.value=v;
ti.isMinor=false;
this.majorTicks.push(ti);
if(_5<this.majorTickValues.length-1){
_7=Number(v);
_6=(Number(this.majorTickValues[i+1])-_7)/(this.minorTickCount+1);
for(var j=1;j<=this.minorTickCount;j++){
ti={scaler:this};
ti.isMinor=true;
ti.position=_5*_3+j*_4;
ti.value=_7+_6*j;
this.minorTicks.push(ti);
}
}
_5++;
}
return this.majorTicks.concat(this.minorTicks);
},positionForValue:function(_8){
if(!this.majorTickValues){
return 0;
}
if(!this.majorTicks){
this.computeTicks();
}
var _9=this._getMinMax(_8);
var _a=(_8-_9[0].value)/(_9[1].value-_9[0].value);
return _9[0].position+_a*(_9[1].position-_9[0].position);
},valueForPosition:function(_b){
if(this.majorTicks==null){
return 0;
}
var _c=this._getMinMax(_b,"position");
var _d=(_b-_c[0].position)/(_c[1].position-_c[0].position);
return _c[0].value+_d*(_c[1].value-_c[0].value);
},_getMinMax:function(v,_e){
if(!_e){
_e="value";
}
var _f=[];
var pre;
var _10;
var _11=0;
var _12=this.majorTicks.length-1;
var _13;
if(v<=this.majorTicks[0][_e]||v>=this.majorTicks[_12][_e]){
_f[0]=this.majorTicks[0];
_f[1]=this.majorTicks[_12];
return _f;
}
while(true){
_13=Math.floor((_11+_12)/2);
if(this.majorTicks[_13][_e]<=v){
if(this.majorTicks[_13+1][_e]>=v){
_f[0]=this.majorTicks[_13];
_f[1]=this.majorTicks[_13+1];
return _f;
}else{
_11=_13+1;
continue;
}
}else{
_12=_13;
continue;
}
}
}});
});
