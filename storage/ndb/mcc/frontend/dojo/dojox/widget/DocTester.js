//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/string,dijit/_Widget,dijit/_Templated,dojox/form/BusyButton,dojox/testing/DocTest"],function(_1,_2,_3){
_2.provide("dojox.widget.DocTester");
_2.require("dojo.string");
_2.require("dijit._Widget");
_2.require("dijit._Templated");
_2.require("dojox.form.BusyButton");
_2.require("dojox.testing.DocTest");
_2.declare("dojox.widget.DocTester",[_1._Widget,_1._Templated],{templateString:_2.cache("dojox.widget","DocTester/DocTester.html","<div dojoAttachPoint=\"domNode\" class=\"dojoxDocTester\">\n\t<div dojoAttachPoint=\"containerNode\"></div>\n\t<button dojoType=\"dojox.form.BusyButton\" busyLabel=\"Testing...\" dojoAttachPoint=\"runButtonNode\">Run tests</button>\n\t<button dojoType=\"dijit.form.Button\" dojoAttachPoint=\"resetButtonNode\" style=\"display:none;\">Reset</button>\n\t<span>\n\t\t<span dojoAttachPoint=\"numTestsNode\">0</span> tests,\n\t\t<span dojoAttachPoint=\"numTestsOkNode\">0</span> passed,\n\t\t<span dojoAttachPoint=\"numTestsNokNode\">0</span> failed\n\t</span>\n</div>"),widgetsInTemplate:true,_fillContent:function(_4){
var _5=_4.innerHTML;
this.doctests=new _3.testing.DocTest();
this.tests=this.doctests.getTestsFromString(this._unescapeHtml(_5));
var _6=_2.map(this.tests,"return item.line-1");
var _7=_5.split("\n");
var _8="<div class=\"actualResult\">FAILED, actual result was: <span class=\"result\"></span></div>";
var _9="<pre class=\"testCase testNum0 odd\">";
for(var i=0;i<_7.length;i++){
var _a=_2.indexOf(_6,i);
if(_a>0&&_a!=-1){
var _b=_a%2?"even":"odd";
_9+=_8;
_9+="</pre><pre class=\"testCase testNum"+_a+" "+_b+"\">";
}
_9+=_7[i].replace(/^\s+/,"")+"\n";
}
_9+=_8+"</pre>";
this.containerNode.innerHTML=_9;
},postCreate:function(){
this.inherited("postCreate",arguments);
_2.connect(this.runButtonNode,"onClick",_2.hitch(this,"runTests"));
_2.connect(this.resetButtonNode,"onClick",_2.hitch(this,"reset"));
this.numTestsNode.innerHTML=this.tests.length;
},runTests:function(){
var _c={ok:0,nok:0};
for(var i=0;i<this.tests.length;i++){
var _d=this.doctests.runTest(this.tests[i].commands,this.tests[i].expectedResult);
_2.query(".testNum"+i,this.domNode).addClass(_d.success?"resultOk":"resultNok");
if(!_d.success){
_c.nok++;
this.numTestsNokNode.innerHTML=_c.nok;
var _e=_2.query(".testNum"+i+" .actualResult",this.domNode)[0];
_2.style(_e,"display","inline");
_2.query(".result",_e)[0].innerHTML=_2.toJson(_d.actualResult);
}else{
_c.ok++;
this.numTestsOkNode.innerHTML=_c.ok;
}
}
this.runButtonNode.cancel();
_2.style(this.runButtonNode.domNode,"display","none");
_2.style(this.resetButtonNode.domNode,"display","");
},reset:function(){
_2.style(this.runButtonNode.domNode,"display","");
_2.style(this.resetButtonNode.domNode,"display","none");
this.numTestsOkNode.innerHTML="0";
this.numTestsNokNode.innerHTML="0";
_2.query(".actualResult",this.domNode).style("display","none");
_2.query(".testCase",this.domNode).removeClass("resultOk").removeClass("resultNok");
},_unescapeHtml:function(_f){
_f=String(_f).replace(/&amp;/gm,"&").replace(/&lt;/gm,"<").replace(/&gt;/gm,">").replace(/&quot;/gm,"\"");
return _f;
}});
});
