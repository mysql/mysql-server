//>>built
define("dojox/image/Badge",["dojo","dijit","dojox/main","dijit/_Widget","dijit/_TemplatedMixin","dojo/fx/easing"],function(_1,_2,_3){
_1.experimental("dojox.image.Badge");
_1.getObject("image",true,_3);
_1.declare("dojox.image.Badge",[_2._Widget,_2._TemplatedMixin],{baseClass:"dojoxBadge",templateString:"<div class=\"dojoxBadge\" dojoAttachPoint=\"containerNode\"></div>",children:"div.dojoxBadgeImage",rows:4,cols:5,cellSize:50,cellMargin:1,delay:2000,threads:1,easing:"dojo.fx.easing.backOut",startup:function(){
if(this._started){
return;
}
if(_1.isString(this.easing)){
this.easing=_1.getObject(this.easing);
}
this.inherited(arguments);
this._init();
},_init:function(){
var _4=0,_5=this.cellSize;
_1.style(this.domNode,{width:_5*this.cols+"px",height:_5*this.rows+"px"});
this._nl=_1.query(this.children,this.containerNode).forEach(function(n,_6){
var _7=_6%this.cols,t=_4*_5,l=_7*_5,m=this.cellMargin*2;
_1.style(n,{top:t+"px",left:l+"px",width:_5-m+"px",height:_5-m+"px"});
if(_7==this.cols-1){
_4++;
}
_1.addClass(n,this.baseClass+"Image");
},this);
var l=this._nl.length;
while(this.threads--){
var s=Math.floor(Math.random()*l);
setTimeout(_1.hitch(this,"_enbiggen",{target:this._nl[s]}),this.delay*this.threads);
}
},_getCell:function(n){
var _8=this._nl.indexOf(n);
if(_8>=0){
var _9=_8%this.cols;
var _a=Math.floor(_8/this.cols);
return {x:_9,y:_a,n:this._nl[_8],io:_8};
}else{
return undefined;
}
},_getImage:function(){
return "url('')";
},_enbiggen:function(e){
var _b=this._getCell(e.target||e);
if(_b){
var m=this.cellMargin,_c=(this.cellSize*2)-(m*2),_d={height:_c,width:_c};
var _e=function(){
return Math.round(Math.random());
};
if(_b.x==this.cols-1||(_b.x>0&&_e())){
_d.left=this.cellSize*(_b.x-m);
}
if(_b.y==this.rows-1||(_b.y>0&&_e())){
_d.top=this.cellSize*(_b.y-m);
}
var bc=this.baseClass;
_1.addClass(_b.n,bc+"Top");
_1.addClass(_b.n,bc+"Seen");
_1.animateProperty({node:_b.n,properties:_d,onEnd:_1.hitch(this,"_loadUnder",_b,_d),easing:this.easing}).play();
}
},_loadUnder:function(_f,_10){
var idx=_f.io;
var _11=[];
var _12=(_10.left>=0);
var _13=(_10.top>=0);
var c=this.cols,e=idx+(_12?-1:1),f=idx+(_13?-c:c),g=(_13?(_12?e-c:f+1):(_12?f-1:e+c)),bc=this.baseClass;
_1.forEach([e,f,g],function(x){
var n=this._nl[x];
if(n){
if(_1.hasClass(n,bc+"Seen")){
_1.removeClass(n,bc+"Seen");
}
}
},this);
setTimeout(_1.hitch(this,"_disenbiggen",_f,_10),this.delay*1.25);
},_disenbiggen:function(_14,_15){
if(_15.top>=0){
_15.top+=this.cellSize;
}
if(_15.left>=0){
_15.left+=this.cellSize;
}
var _16=this.cellSize-(this.cellMargin*2);
_1.animateProperty({node:_14.n,properties:_1.mixin(_15,{width:_16,height:_16}),onEnd:_1.hitch(this,"_cycle",_14,_15)}).play(5);
},_cycle:function(_17,_18){
var bc=this.baseClass;
_1.removeClass(_17.n,bc+"Top");
var ns=this._nl.filter(function(n){
return !_1.hasClass(n,bc+"Seen");
});
var c=ns[Math.floor(Math.random()*ns.length)];
setTimeout(_1.hitch(this,"_enbiggen",{target:c}),this.delay/2);
}});
return _3.image.Badge;
});
