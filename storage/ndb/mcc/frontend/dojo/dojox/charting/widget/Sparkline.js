//>>built
define("dojox/charting/widget/Sparkline",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/query","./Chart","../themes/GreySkies","../plot2d/Lines","dojo/dom-prop"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_3("dojox.charting.widget.Sparkline",_5,{theme:_6,margins:{l:0,r:0,t:0,b:0},type:"Lines",valueFn:"Number(x)",store:"",field:"",query:"",queryOptions:"",start:"0",count:"Infinity",sort:"",data:"",name:"default",buildRendering:function(){
var n=this.srcNodeRef;
if(!n.childNodes.length||!_4("> .axis, > .plot, > .action, > .series",n).length){
var _9=document.createElement("div");
_8.set(_9,{"class":"plot","name":"default","type":this.type});
n.appendChild(_9);
var _a=document.createElement("div");
_8.set(_a,{"class":"series",plot:"default",name:this.name,start:this.start,count:this.count,valueFn:this.valueFn});
_2.forEach(["store","field","query","queryOptions","sort","data"],function(i){
if(this[i].length){
_8.set(_a,i,this[i]);
}
},this);
n.appendChild(_a);
}
this.inherited(arguments);
}});
});
