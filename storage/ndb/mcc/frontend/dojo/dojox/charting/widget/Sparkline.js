//>>built
define("dojox/charting/widget/Sparkline",["dojo/_base/array","dojo/_base/declare","dojo/query","./Chart","../themes/GreySkies","../plot2d/Lines","dojo/dom-prop"],function(_1,_2,_3,_4,_5,_6,_7){
_2("dojox.charting.widget.Sparkline",_4,{theme:_5,margins:{l:0,r:0,t:0,b:0},type:"Lines",valueFn:"Number(x)",store:"",field:"",query:"",queryOptions:"",start:"0",count:"Infinity",sort:"",data:"",name:"default",buildRendering:function(){
var n=this.srcNodeRef;
if(!n.childNodes.length||!_3("> .axis, > .plot, > .action, > .series",n).length){
var _8=document.createElement("div");
_7.set(_8,{"class":"plot","name":"default","type":this.type});
n.appendChild(_8);
var _9=document.createElement("div");
_7.set(_9,{"class":"series",plot:"default",name:this.name,start:this.start,count:this.count,valueFn:this.valueFn});
_1.forEach(["store","field","query","queryOptions","sort","data"],function(i){
if(this[i].length){
_7.set(_9,i,this[i]);
}
},this);
n.appendChild(_9);
}
this.inherited(arguments);
}});
});
