//>>built
define("dojox/collections/Set",["./_base","./ArrayList"],function(_1,_2){
_1.Set=new (function(){
function _3(_4){
if(_4.constructor==Array){
return new _2(_4);
}
return _4;
};
this.union=function(_5,_6){
_5=_3(_5);
_6=_3(_6);
var _7=new _2(_5.toArray());
var e=_6.getIterator();
while(!e.atEnd()){
var _8=e.get();
if(!_7.contains(_8)){
_7.add(_8);
}
}
return _7;
};
this.intersection=function(_9,_a){
_9=_3(_9);
_a=_3(_a);
var _b=new _2();
var e=_a.getIterator();
while(!e.atEnd()){
var _c=e.get();
if(_9.contains(_c)){
_b.add(_c);
}
}
return _b;
};
this.difference=function(_d,_e){
_d=_3(_d);
_e=_3(_e);
var _f=new _2();
var e=_d.getIterator();
while(!e.atEnd()){
var _10=e.get();
if(!_e.contains(_10)){
_f.add(_10);
}
}
return _f;
};
this.isSubSet=function(_11,_12){
_11=_3(_11);
_12=_3(_12);
var e=_11.getIterator();
while(!e.atEnd()){
if(!_12.contains(e.get())){
return false;
}
}
return true;
};
this.isSuperSet=function(_13,_14){
_13=_3(_13);
_14=_3(_14);
var e=_14.getIterator();
while(!e.atEnd()){
if(!_13.contains(e.get())){
return false;
}
}
return true;
};
})();
return _1.Set;
});
