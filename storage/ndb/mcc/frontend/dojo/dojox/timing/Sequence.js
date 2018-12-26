//>>built
define("dojox/timing/Sequence",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/lang","./_base"],function(_1){
_1.experimental("dojox.timing.Sequence");
_1.declare("dojox.timing.Sequence",null,{_goOnPause:0,_running:false,constructor:function(){
this._defsResolved=[];
},go:function(_2,_3){
this._running=true;
_1.forEach(_2,function(_4){
if(_4.repeat>1){
var _5=_4.repeat;
for(var j=0;j<_5;j++){
_4.repeat=1;
this._defsResolved.push(_4);
}
}else{
this._defsResolved.push(_4);
}
},this);
var _6=_2[_2.length-1];
if(_3){
this._defsResolved.push({func:_3});
}
this._defsResolved.push({func:[this.stop,this]});
this._curId=0;
this._go();
},_go:function(){
if(!this._running){
return;
}
var _7=this._defsResolved[this._curId];
this._curId+=1;
function _8(_9){
var _a=null;
if(_1.isArray(_9)){
if(_9.length>2){
_a=_9[0].apply(_9[1],_9.slice(2));
}else{
_a=_9[0].apply(_9[1]);
}
}else{
_a=_9();
}
return _a;
};
if(this._curId>=this._defsResolved.length){
_8(_7.func);
return;
}
if(_7.pauseAfter){
if(_8(_7.func)!==false){
setTimeout(_1.hitch(this,"_go"),_7.pauseAfter);
}else{
this._goOnPause=_7.pauseAfter;
}
}else{
if(_7.pauseBefore){
var x=_1.hitch(this,function(){
if(_8(_7.func)!==false){
this._go();
}
});
setTimeout(x,_7.pauseBefore);
}else{
if(_8(_7.func)!==false){
this._go();
}
}
}
},goOn:function(){
if(this._goOnPause){
setTimeout(_1.hitch(this,"_go"),this._goOnPause);
this._goOnPause=0;
}else{
this._go();
}
},stop:function(){
this._running=false;
}});
return dojox.timing.Sequence;
});
