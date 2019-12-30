//>>built
define("dojox/widget/DocTester",["dojo","dijit","dojox","dojo/require!dojo/string,dijit/_Widget,dijit/_Templated,dojox/form/BusyButton,dojox/testing/DocTest"],function(_1,_2,_3){
_1.provide("dojox.widget.DocTester");
_1.require("dojo.string");
_1.require("dijit._Widget");
_1.require("dijit._Templated");
_1.require("dojox.form.BusyButton");
_1.require("dojox.testing.DocTest");
_1.declare("dojox.widget.DocTester",[_2._Widget,_2._Templated],{templateString:_1.cache("dojox.widget","DocTester/DocTester.html","<div dojoAttachPoint=\"domNode\" class=\"dojoxDocTester\">\n\t<div dojoAttachPoint=\"containerNode\"></div>\n\t<button dojoType=\"dojox.form.BusyButton\" busyLabel=\"Testing...\" dojoAttachPoint=\"runButtonNode\">Run tests</button>\n\t<button dojoType=\"dijit.form.Button\" dojoAttachPoint=\"resetButtonNode\" style=\"display:none;\">Reset</button>\n\t<span>\n\t\t<span dojoAttachPoint=\"numTestsNode\">0</span> tests,\n\t\t<span dojoAttachPoint=\"numTestsOkNode\">0</span> passed,\n\t\t<span dojoAttachPoint=\"numTestsNokNode\">0</span> failed\n\t</span>\n</div>"),widgetsInTemplate:true,_fillContent:function(_4){
var _5=_4.innerHTML;
this.doctests=new _3.testing.DocTest();
this.tests=this.doctests.getTestsFromString(this._unescapeHtml(_5));
var _6=_1.map(this.tests,"return item.line-1");
var _7=_5.split("\n");
var _8="<div class=\"actualResult\">FAILED, actual result was: <span class=\"result\"></span></div>";
var _9="<pre class=\"testCase testNum0 odd\">";
for(var i=0;i<_7.length;i++){
var _a=_1.indexOf(_6,i);
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
_1.connect(this.runButtonNode,"onClick",_1.hitch(this,"runTests"));
_1.connect(this.resetButtonNode,"onClick",_1.hitch(this,"reset"));
this.numTestsNode.innerHTML=this.tests.length;
},runTests:function(){
var _c={ok:0,nok:0};
for(var i=0;i<this.tests.length;i++){
var _d=this.doctests.runTest(this.tests[i].commands,this.tests[i].expectedResult);
_1.query(".testNum"+i,this.domNode).addClass(_d.success?"resultOk":"resultNok");
if(!_d.success){
_c.nok++;
this.numTestsNokNode.innerHTML=_c.nok;
var _e=_1.query(".testNum"+i+" .actualResult",this.domNode)[0];
_1.style(_e,"display","inline");
_1.query(".result",_e)[0].innerHTML=_1.toJson(_d.actualResult);
}else{
_c.ok++;
this.numTestsOkNode.innerHTML=_c.ok;
}
}
this.runButtonNode.cancel();
_1.style(this.runButtonNode.domNode,"display","none");
_1.style(this.resetButtonNode.domNode,"display","");
},reset:function(){
_1.style(this.runButtonNode.domNode,"display","");
_1.style(this.resetButtonNode.domNode,"display","none");
this.numTestsOkNode.innerHTML="0";
this.numTestsNokNode.innerHTML="0";
_1.query(".actualResult",this.domNode).style("display","none");
_1.query(".testCase",this.domNode).removeClass("resultOk").removeClass("resultNok");
},_unescapeHtml:function(_f){
_f=String(_f).replace(/&amp;/gm,"&").replace(/&lt;/gm,"<").replace(/&gt;/gm,">").replace(/&quot;/gm,"\"");
return _f;
}});
});
