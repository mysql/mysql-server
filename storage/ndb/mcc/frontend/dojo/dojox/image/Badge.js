//>>built
define("dojox/image/Badge",["dojo","dijit","dojox/main","dijit/_Widget","dijit/_TemplatedMixin","dojo/fx/easing"],function(_1,_2,_3,_4,_5){
_1.experimental("dojox.image.Badge");
_1.getObject("image",true,_3);
_1.declare("dojox.image.Badge",[_4,_5],{baseClass:"dojoxBadge",templateString:"<div class=\"dojoxBadge\" dojoAttachPoint=\"containerNode\"></div>",children:"div.dojoxBadgeImage",rows:4,cols:5,cellSize:50,cellMargin:1,delay:2000,threads:1,easing:"dojo.fx.easing.backOut",startup:function(){
if(this._started){
return;
}
if(_1.isString(this.easing)){
this.easing=_1.getObject(this.easing);
}
this.inherited(arguments);
this._init();
},_init:function(){
var _6=0,_7=this.cellSize;
_1.style(this.domNode,{width:_7*this.cols+"px",height:_7*this.rows+"px"});
this._nl=_1.query(this.children,this.containerNode).forEach(function(n,_8){
var _9=_8%this.cols,t=_6*_7,l=_9*_7,m=this.cellMargin*2;
_1.style(n,{top:t+"px",left:l+"px",width:_7-m+"px",height:_7-m+"px"});
if(_9==this.cols-1){
_6++;
}
_1.addClass(n,this.baseClass+"Image");
},this);
var l=this._nl.length;
while(this.threads--){
var s=Math.floor(Math.random()*l);
setTimeout(_1.hitch(this,"_enbiggen",{target:this._nl[s]}),this.delay*this.threads);
}
},_getCell:function(n){
var _a=this._nl.indexOf(n);
if(_a>=0){
var _b=_a%this.cols;
var _c=Math.floor(_a/this.cols);
return {x:_b,y:_c,n:this._nl[_a],io:_a};
}else{
return undefined;
}
},_getImage:function(){
return "url('')";
},_enbiggen:function(e){
var _d=this._getCell(e.target||e);
if(_d){
var m=this.cellMargin,_e=(this.cellSize*2)-(m*2),_f={height:_e,width:_e};
var _10=function(){
return Math.round(Math.random());
};
if(_d.x==this.cols-1||(_d.x>0&&_10())){
_f.left=this.cellSize*(_d.x-m);
}
if(_d.y==this.rows-1||(_d.y>0&&_10())){
_f.top=this.cellSize*(_d.y-m);
}
var bc=this.baseClass;
_1.addClass(_d.n,bc+"Top");
_1.addClass(_d.n,bc+"Seen");
_1.animateProperty({node:_d.n,properties:_f,onEnd:_1.hitch(this,"_loadUnder",_d,_f),easing:this.easing}).play();
}
},_loadUnder:function(_11,_12){
var idx=_11.io;
var _13=[];
var _14=(_12.left>=0);
var _15=(_12.top>=0);
var c=this.cols,e=idx+(_14?-1:1),f=idx+(_15?-c:c),g=(_15?(_14?e-c:f+1):(_14?f-1:e+c)),bc=this.baseClass;
_1.forEach([e,f,g],function(x){
var n=this._nl[x];
if(n){
if(_1.hasClass(n,bc+"Seen")){
_1.removeClass(n,bc+"Seen");
}
}
},this);
setTimeout(_1.hitch(this,"_disenbiggen",_11,_12),this.delay*1.25);
},_disenbiggen:function(_16,_17){
if(_17.top>=0){
_17.top+=this.cellSize;
}
if(_17.left>=0){
_17.left+=this.cellSize;
}
var _18=this.cellSize-(this.cellMargin*2);
_1.animateProperty({node:_16.n,properties:_1.mixin(_17,{width:_18,height:_18}),onEnd:_1.hitch(this,"_cycle",_16,_17)}).play(5);
},_cycle:function(_19,_1a){
var bc=this.baseClass;
_1.removeClass(_19.n,bc+"Top");
var ns=this._nl.filter(function(n){
return !_1.hasClass(n,bc+"Seen");
});
var c=ns[Math.floor(Math.random()*ns.length)];
setTimeout(_1.hitch(this,"_enbiggen",{target:c}),this.delay/2);
}});
return _3.image.Badge;
});
