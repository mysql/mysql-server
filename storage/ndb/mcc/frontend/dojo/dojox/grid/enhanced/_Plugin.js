//>>built
define("dojox/grid/enhanced/_Plugin",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/_base/connect","../EnhancedGrid"],function(_1,_2,_3,_4,_5){
return _3("dojox.grid.enhanced._Plugin",null,{name:"plugin",grid:null,option:{},_connects:[],_subscribes:[],privates:{},constructor:function(_6,_7){
this.grid=_6;
this.option=_7;
this._connects=[];
this._subscribes=[];
this.privates=_2.mixin({},dojox.grid.enhanced._Plugin.prototype);
this.init();
},init:function(){
},onPreInit:function(){
},onPostInit:function(){
},onStartUp:function(){
},connect:function(_8,_9,_a){
var _b=_5.connect(_8,_9,this,_a);
this._connects.push(_b);
return _b;
},disconnect:function(_c){
_4.some(this._connects,function(_d,i,_e){
if(_d==_c){
_5.disconnect(_c);
_e.splice(i,1);
return true;
}
return false;
});
},subscribe:function(_f,_10){
var _11=_5.subscribe(_f,this,_10);
this._subscribes.push(_11);
return _11;
},unsubscribe:function(_12){
_4.some(this._subscribes,function(_13,i,_14){
if(_13==_12){
_5.unsubscribe(_12);
_14.splice(i,1);
return true;
}
return false;
});
},onSetStore:function(_15){
},destroy:function(){
_4.forEach(this._connects,_5.disconnect);
_4.forEach(this._subscribes,_5.unsubscribe);
delete this._connects;
delete this._subscribes;
delete this.option;
delete this.privates;
}});
});
