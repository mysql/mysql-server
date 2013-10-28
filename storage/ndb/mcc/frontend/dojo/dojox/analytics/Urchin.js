//>>built
define("dojox/analytics/Urchin",["dojo/_base/lang","dojo/_base/declare","dojo/_base/window","dojo/_base/config","dojo/dom-construct"],function(_1,_2,_3,_4,_5){
return _2("dojox.analytics.Urchin",null,{acct:"",constructor:function(_6){
this.tracker=null;
_1.mixin(this,_6);
this.acct=this.acct||_4.urchin;
var re=/loaded|complete/,_7=("https:"==_3.doc.location.protocol)?"https://ssl.":"http://www.",h=_3.doc.getElementsByTagName("head")[0],n=_5.create("script",{src:_7+"google-analytics.com/ga.js"},h);
n.onload=n.onreadystatechange=_1.hitch(this,function(e){
if(e&&e.type=="load"||re.test(n.readyState)){
n.onload=n.onreadystatechange=null;
this._gotGA();
h.removeChild(n);
}
});
},_gotGA:function(){
this.tracker=_gat._getTracker(this.acct);
this.GAonLoad.apply(this,arguments);
},GAonLoad:function(){
this.trackPageView();
},trackPageView:function(_8){
this.tracker._trackPageview.apply(this,arguments);
}});
});
