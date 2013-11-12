//>>built
require({cache:{"url:dojox/calc/templates/Grapher.html":"<div>\n<div data-dojo-attach-point=\"chartsParent\" class=\"dojoxCalcChartHolder\"></div>\n<div data-dojo-attach-point=\"outerDiv\">\n<div data-dojo-type=\"dijit.form.DropDownButton\" data-dojo-attach-point=\"windowOptions\" class=\"dojoxCalcDropDownForWindowOptions\" title=\"Window Options\">\n\t<div>Window Options</div>\n\t<div data-dojo-type=\"dijit.TooltipDialog\" data-dojo-attach-point=\"windowOptionsInside\" class=\"dojoxCalcTooltipDialogForWindowOptions\" title=\"\">\n\t\t<table class=\"dojoxCalcGraphOptionTable\">\n\t\t\t<tr>\n\t\t\t\t<td>\n\t\t\t\t\tWidth:\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t\t\t<input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-point=\"graphWidth\" class=\"dojoxCalcGraphWidth\" value=\"500\" />\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t\t\tHeight:\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t\t\t<input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-point=\"graphHeight\" class=\"dojoxCalcGraphHeight\" value=\"500\" />\n\t\t\t\t</td>\n\t\t\t</tr>\n\t\t\t<tr>\n\t\t\t\t<td>\n\t\t\t\t\tX >=\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t\t\t<input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-point=\"graphMinX\" class=\"dojoxCalcGraphMinX\" value=\"-10\" />\n\t\t\t\t</td>\n\n\t\t\t\t<td>\n\t\t\t\t\tX <=\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t\t\t<input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-point=\"graphMaxX\" class=\"dojoxCalcGraphMaxX\" value=\"10\" />\n\t\t\t\t</td>\n\t\t\t</tr>\n\t\t\t<tr>\n\t\t\t\t<td>\n\t\t\t\t\tY >=\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t\t\t<input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-point=\"graphMinY\" class=\"dojoxCalcGraphMinY\" value=\"-10\" />\n\t\t\t\t</td>\n\n\t\t\t\t<td>\n\t\t\t\t\tY <=\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t\t\t<input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-point=\"graphMaxY\" class=\"dojoxCalcGraphMaxY\" value=\"10\" />\n\t\t\t\t</td>\n\t\t\t</tr>\n\t\t</table>\n\t</div>\n</div>\n\n<BR>\n\n<div class=\"dojoxCalcGrapherFuncOuterDiv\">\n\t<table class=\"dojoxCalcGrapherFuncTable\" data-dojo-attach-point=\"graphTable\">\n\t</table>\n</div>\n\n<div data-dojo-type=\"dijit.form.DropDownButton\" data-dojo-attach-point='addFuncButton' class=\"dojoxCalcDropDownAddingFunction\">\n\t<div>Add Function</div>\n\t<div data-dojo-type=\"dijit.TooltipDialog\" data-dojo-attach-point=\"addFuncInside\" class=\"dojoxCalcTooltipDialogAddingFunction\" title=\"\">\n\t\t<table class=\"dojoxCalcGrapherModeTable\">\n\t\t\t<tr>\n\t\t\t\t<td>\n\t\t\t\t\tMode:\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t\t\t<select data-dojo-type=\"dijit.form.Select\" data-dojo-attach-point=\"funcMode\" class=\"dojoxCalcFunctionModeSelector\">\n\t\t\t\t\t\t<option value=\"y=\" selected=\"selected\">y=</option>\n\t\t\t\t\t\t<option value=\"x=\">x=</option>\n\t\t\t\t\t</select>\n\t\t\t\t</td>\n\t\t\t\t<td>\n\t\t\t</tr>\n\t\n\t\t\t<tr>\n\t\t\t\t<td>\n\t\t\t\t\t<input data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"createFunc\" class=\"dojoxCalcAddFunctionButton\" label=\"Create\" />\n\t\t\t\t</td>\n\t\t\t</tr>\n\t\t</table>\n\t</div>\n</div>\n<BR>\n<BR>\n<table class=\"dijitInline dojoxCalcGrapherLayout\">\n\t<tr>\n\t\t<td class=\"dojoxCalcGrapherButtonContainer\">\n\t\t\t<input data-dojo-type=\"dijit.form.Button\" class=\"dojoxCalcGrapherButton\" data-dojo-attach-point='selectAllButton' label=\"Select All\" />\n\t\t</td>\n\t\t<td class=\"dojoxCalcGrapherButtonContainer\">\n\t\t\t<input data-dojo-type=\"dijit.form.Button\" class=\"dojoxCalcGrapherButton\" data-dojo-attach-point='deselectAllButton' label=\"Deselect All\" />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcGrapherButtonContainer\">\n\t\t\t<input data-dojo-type=\"dijit.form.Button\" class=\"dojoxCalcGrapherButton\" data-dojo-attach-point='drawButton'label=\"Draw Selected\" />\n\t\t</td>\n\t\t<td class=\"dojoxCalcGrapherButtonContainer\">\n\t\t\t<input data-dojo-type=\"dijit.form.Button\" class=\"dojoxCalcGrapherButton\" data-dojo-attach-point='eraseButton' label=\"Erase Selected\" />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcGrapherButtonContainer\">\n\t\t\t<input data-dojo-type=\"dijit.form.Button\" class=\"dojoxCalcGrapherButton\" data-dojo-attach-point='deleteButton' label=\"Delete Selected\" />\n\t\t</td>\n\t\t<td class=\"dojoxCalcGrapherButtonContainer\">\n\t\t\t<input data-dojo-type=\"dijit.form.Button\" class=\"dojoxCalcGrapherButton\" data-dojo-attach-point='closeButton' label=\"Close\" />\n\t\t</td>\n\t</tr>\n</table>\n</div>\n</div>\n"}});
define("dojox/calc/Grapher",["dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-construct","dojo/dom-class","dojo/dom-style","dijit/_WidgetBase","dijit/_WidgetsInTemplateMixin","dijit/_TemplatedMixin","dojox/math/_base","dijit/registry","dijit/form/DropDownButton","dijit/TooltipDialog","dijit/form/TextBox","dijit/form/CheckBox","dijit/ColorPalette","dojox/charting/Chart","dojox/charting/axis2d/Default","dojox/charting/plot2d/Default","dojox/charting/plot2d/Lines","dojox/charting/themes/Tufte","dojo/colors","dojo/text!./templates/Grapher.html","dojox/calc/_Executor","dijit/form/Button","dijit/form/Select"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18){
var _19=1e-15/9,_1a=1e+200,_1b=Math.log(2),_1c={graphNumber:0,fOfX:true,color:{stroke:"black"}};
var _1d=_1("dojox.calc.Grapher",[_7,_9,_8],{templateString:_17,addXYAxes:function(_1e){
return _1e.addAxis("x",{max:parseInt(this.graphMaxX.get("value")),min:parseInt(this.graphMinX.get("value")),majorLabels:true,minorLabels:true,minorTicks:false,microTicks:false,htmlLabels:true,labelFunc:function(_1f){
return _1f;
},maxLabelSize:30,fixUpper:"major",fixLower:"major",majorTick:{length:3}}).addAxis("y",{max:parseInt(this.graphMaxY.get("value")),min:parseInt(this.graphMinY.get("value")),labelFunc:function(_20){
return _20;
},maxLabelSize:50,vertical:true,microTicks:false,minorTicks:true,majorTick:{stroke:"black",length:3}});
},selectAll:function(){
for(var i=0;i<this.rowCount;i++){
this.array[i][this.checkboxIndex].set("checked",true);
}
},deselectAll:function(){
for(var i=0;i<this.rowCount;i++){
this.array[i][this.checkboxIndex].set("checked",false);
}
},drawOne:function(i){
},onDraw:function(){
},erase:function(i){
var _21=0;
var _22="Series "+this.array[i][this.funcNumberIndex]+"_"+_21;
while(_22 in this.array[i][this.chartIndex].runs){
this.array[i][this.chartIndex].removeSeries(_22);
_21++;
_22="Series "+this.array[i][this.funcNumberIndex]+"_"+_21;
}
this.array[i][this.chartIndex].render();
this.setStatus(i,"Hidden");
},onErase:function(){
for(var i=0;i<this.rowCount;i++){
if(this.array[i][this.checkboxIndex].get("checked")){
this.erase(i);
}
}
},onDelete:function(){
for(var i=0;i<this.rowCount;i++){
if(this.array[i][this.checkboxIndex].get("checked")){
this.erase(i);
for(var k=0;k<this.functionRef;k++){
if(this.array[i][k]&&this.array[i][k]["destroy"]){
this.array[i][k].destroy();
}
}
this.graphTable.deleteRow(i);
this.array.splice(i,1);
this.rowCount--;
i--;
}
}
},checkboxIndex:0,functionMode:1,expressionIndex:2,colorIndex:3,dropDownIndex:4,tooltipIndex:5,colorBoxFieldsetIndex:6,statusIndex:7,chartIndex:8,funcNumberIndex:9,evaluatedExpression:10,functionRef:11,createFunction:function(){
var tr=this.graphTable.insertRow(-1);
this.array[tr.rowIndex]=[];
var td=tr.insertCell(-1);
var d=_4.create("div");
td.appendChild(d);
var _23=new _f({},d);
this.array[tr.rowIndex][this.checkboxIndex]=_23;
_5.add(d,"dojoxCalcCheckBox");
td=tr.insertCell(-1);
var _24=this.funcMode.get("value");
d=_3.doc.createTextNode(_24);
td.appendChild(d);
this.array[tr.rowIndex][this.functionMode]=_24;
td=tr.insertCell(-1);
d=_4.create("div");
td.appendChild(d);
var _25=new _e({},d);
this.array[tr.rowIndex][this.expressionIndex]=_25;
_5.add(d,"dojoxCalcExpressionBox");
var b=_4.create("div");
var _26=new _10({changedColor:this.changedColor},b);
_5.add(b,"dojoxCalcColorPalette");
this.array[tr.rowIndex][this.colorIndex]=_26;
var c=_4.create("div");
var _27=new _d({content:_26},c);
this.array[tr.rowIndex][this.tooltipIndex]=_27;
_5.add(c,"dojoxCalcContainerOfColor");
td=tr.insertCell(-1);
d=_4.create("div");
td.appendChild(d);
var _28=_4.create("fieldset");
_6.set(_28,{backgroundColor:"black",width:"1em",height:"1em",display:"inline"});
this.array[tr.rowIndex][this.colorBoxFieldsetIndex]=_28;
var _29=new _c({label:"Color ",dropDown:_27},d);
_29.containerNode.appendChild(_28);
this.array[tr.rowIndex][this.dropDownIndex]=_29;
_5.add(d,"dojoxCalcDropDownForColor");
td=tr.insertCell(-1);
d=_4.create("fieldset");
d.innerHTML="Hidden";
this.array[tr.rowIndex][this.statusIndex]=d;
_5.add(d,"dojoxCalcStatusBox");
td.appendChild(d);
d=_4.create("div");
_6.set(d,{position:"absolute",left:"0px",top:"0px"});
this.chartsParent.appendChild(d);
this.array[tr.rowIndex][this.chartNodeIndex]=d;
_5.add(d,"dojoxCalcChart");
var _2a=new dojox.charting.Chart(d).setTheme(dojox.charting.themes.Tufte).addPlot("default",{type:"Lines",shadow:{dx:1,dy:1,width:2,color:[0,0,0,0.3]}});
this.addXYAxes(_2a);
this.array[tr.rowIndex][this.chartIndex]=_2a;
_26.set("chart",_2a);
_26.set("colorBox",_28);
_26.set("onChange",_2.hitch(_26,"changedColor"));
this.array[tr.rowIndex][this.funcNumberIndex]=this.funcNumber++;
this.rowCount++;
},setStatus:function(i,_2b){
this.array[i][this.statusIndex].innerHTML=_2b;
},changedColor:function(){
var _2c=this.get("chart");
var _2d=this.get("colorBox");
for(var i=0;i<_2c.series.length;i++){
if(_2c.series[i]["stroke"]){
if(_2c.series[i].stroke["color"]){
_2c.series[i]["stroke"].color=this.get("value");
_2c.dirty=true;
}
}
}
_2c.render();
_6.set(_2d,{backgroundColor:this.get("value")});
},makeDirty:function(){
this.dirty=true;
},checkDirty1:function(){
setTimeout(_2.hitch(this,"checkDirty"),0);
},checkDirty:function(){
if(this.dirty){
for(var i=0;i<this.rowCount;i++){
this.array[i][this.chartIndex].removeAxis("x");
this.array[i][this.chartIndex].removeAxis("y");
this.addXYAxes(this.array[i][this.chartIndex]);
}
this.onDraw();
}
this.dirty=false;
},postCreate:function(){
this.inherited(arguments);
this.createFunc.set("onClick",_2.hitch(this,"createFunction"));
this.selectAllButton.set("onClick",_2.hitch(this,"selectAll"));
this.deselectAllButton.set("onClick",_2.hitch(this,"deselectAll"));
this.drawButton.set("onClick",_2.hitch(this,"onDraw"));
this.eraseButton.set("onClick",_2.hitch(this,"onErase"));
this.deleteButton.set("onClick",_2.hitch(this,"onDelete"));
this.dirty=false;
this.graphWidth.set("onChange",_2.hitch(this,"makeDirty"));
this.graphHeight.set("onChange",_2.hitch(this,"makeDirty"));
this.graphMaxX.set("onChange",_2.hitch(this,"makeDirty"));
this.graphMinX.set("onChange",_2.hitch(this,"makeDirty"));
this.graphMaxY.set("onChange",_2.hitch(this,"makeDirty"));
this.graphMinY.set("onChange",_2.hitch(this,"makeDirty"));
this.windowOptionsInside.set("onClose",_2.hitch(this,"checkDirty1"));
this.funcNumber=0;
this.rowCount=0;
this.array=[];
},startup:function(){
this.inherited(arguments);
var _2e=_b.getEnclosingWidget(this.domNode.parentNode);
if(_2e&&typeof _2e.close=="function"){
this.closeButton.set("onClick",_2.hitch(_2e,"close"));
}else{
_6.set(this.closeButton.domNode,{display:"none"});
}
this.createFunction();
this.array[0][this.checkboxIndex].set("checked",true);
this.onDraw();
this.erase(0);
this.array[0][this.expressionIndex].value="";
}});
return _2.mixin(_18,{draw:function(_2f,_30,_31){
_31=_2.mixin({},_1c,_31);
_2f.fullGeometry();
var x;
var y;
var _32;
if(_31.fOfX==true){
x="x";
y="y";
_32=_18.generatePoints(_30,x,y,_2f.axes.x.scaler.bounds.span,_2f.axes.x.scaler.bounds.lower,_2f.axes.x.scaler.bounds.upper,_2f.axes.y.scaler.bounds.lower,_2f.axes.y.scaler.bounds.upper);
}else{
x="y";
y="x";
_32=_18.generatePoints(_30,x,y,_2f.axes.y.scaler.bounds.span,_2f.axes.y.scaler.bounds.lower,_2f.axes.y.scaler.bounds.upper,_2f.axes.x.scaler.bounds.lower,_2f.axes.x.scaler.bounds.upper);
}
var i=0;
if(_32.length>0){
for(;i<_32.length;i++){
if(_32[i].length>0){
_2f.addSeries("Series "+_31.graphNumber+"_"+i,_32[i],_31.color);
}
}
}
var _33="Series "+_31.graphNumber+"_"+i;
while(_33 in _2f.runs){
_2f.removeSeries(_33);
i++;
_33="Series "+_31.graphNumber+"_"+i;
}
_2f.render();
return _32;
},generatePoints:function(_34,x,y,_35,_36,_37,_38,_39){
var _3a=(1<<Math.ceil(Math.log(_35)/_1b));
var dx=(_37-_36)/_3a,_3b=[],_3c=0,_3d,_3e;
_3b[_3c]=[];
var i=_36,k,p;
for(var _3f=0;_3f<=_3a;i+=dx,_3f++){
p={};
p[x]=i;
p[y]=_34({_name:x,_value:i,_graphing:true});
if(p[x]==null||p[y]==null){
return {};
}
if(isNaN(p[y])||isNaN(p[x])){
continue;
}
_3b[_3c].push(p);
if(_3b[_3c].length==3){
_3d=_40(_41(_3b[_3c][_3b[_3c].length-3],_3b[_3c][_3b[_3c].length-2]),_41(_3b[_3c][_3b[_3c].length-2],_3b[_3c][_3b[_3c].length-1]));
continue;
}
if(_3b[_3c].length<4){
continue;
}
_3e=_40(_41(_3b[_3c][_3b[_3c].length-3],_3b[_3c][_3b[_3c].length-2]),_41(_3b[_3c][_3b[_3c].length-2],_3b[_3c][_3b[_3c].length-1]));
if(_3d.inc!=_3e.inc||_3d.pos!=_3e.pos){
var a=_42(_34,_3b[_3c][_3b[_3c].length-3],_3b[_3c][_3b[_3c].length-1]);
p=_3b[_3c].pop();
_3b[_3c].pop();
for(var j=0;j<a[0].length;j++){
_3b[_3c].push(a[0][j]);
}
for(k=1;k<a.length;k++){
_3b[++_3c]=a.pop();
}
_3b[_3c].push(p);
_3d=_3e;
}
}
while(_3b.length>1){
for(k=0;k<_3b[1].length;k++){
if(_3b[0][_3b[0].length-1][x]==_3b[1][k][x]){
continue;
}
_3b[0].push(_3b[1][k]);
}
_3b.splice(1,1);
}
_3b=_3b[0];
var s=0;
var _43=[[]];
for(k=0;k<_3b.length;k++){
var x1,y1,b,_44;
if(isNaN(_3b[k][y])||isNaN(_3b[k][x])){
while(isNaN(_3b[k][y])||isNaN(_3b[k][x])){
_3b.splice(k,1);
}
_43[++s]=[];
k--;
}else{
if(_3b[k][y]>_39||_3b[k][y]<_38){
if(k>0&&_3b[k-1].y!=_38&&_3b[k-1].y!=_39){
_44=_41(_3b[k-1],_3b[k]);
if(_44>_1a){
_44=_1a;
}else{
if(_44<-_1a){
_44=-_1a;
}
}
if(_3b[k][y]>_39){
y1=_39;
}else{
y1=_38;
}
b=_3b[k][y]-_44*_3b[k][x];
x1=(y1-b)/_44;
p={};
p[x]=x1;
p[y]=_34(x1);
if(p[y]!=y1){
p=_45(_34,_3b[k-1],_3b[k],y1);
}
_43[s].push(p);
_43[++s]=[];
}
var _46=k;
while(k<_3b.length&&(_3b[k][y]>_39||_3b[k][y]<_38)){
k++;
}
if(k>=_3b.length){
if(_43[s].length==0){
_43.splice(s,1);
}
break;
}
if(k>0&&_3b[k].y!=_38&&_3b[k].y!=_39){
_44=_41(_3b[k-1],_3b[k]);
if(_44>_1a){
_44=_1a;
}else{
if(_44<-_1a){
_44=-_1a;
}
}
if(_3b[k-1][y]>_39){
y1=_39;
}else{
y1=_38;
}
b=_3b[k][y]-_44*_3b[k][x];
x1=(y1-b)/_44;
p={};
p[x]=x1;
p[y]=_34(x1);
if(p[y]!=y1){
p=_45(_34,_3b[k-1],_3b[k],y1);
}
_43[s].push(p);
_43[s].push(_3b[k]);
}
}else{
_43[s].push(_3b[k]);
}
}
}
return _43;
function _45(_47,_48,_49,_4a){
while(_48<=_49){
var _4b=(_48[x]+_49[x])/2;
var mid={};
mid[x]=_4b;
mid[y]=_47(mid[x]);
if(_4a==mid[y]||mid[x]==_49[x]||mid[x]==_48[x]){
return mid;
}
var _4c=true;
if(_4a<mid[y]){
_4c=false;
}
if(mid[y]<_49[y]){
if(_4c){
_48=mid;
}else{
_49=mid;
}
}else{
if(mid[y]<_48[y]){
if(!_4c){
_48=mid;
}else{
_49=mid;
}
}
}
}
return NaN;
};
function _42(_4d,_4e,_4f){
var _50=[[],[]],_51=_4e,_52=_4f,_53;
while(_51[x]<=_52[x]){
var _54=(_51[x]+_52[x])/2;
_53={};
_53[x]=_54;
_53[y]=_4d(_54);
var rx=_55(_53[x]);
var _56={};
_56[x]=rx;
_56[y]=_4d(rx);
if(Math.abs(_56[y])>=Math.abs(_53[y])){
_50[0].push(_53);
_51=_56;
}else{
_50[1].unshift(_53);
if(_52[x]==_53[x]){
break;
}
_52=_53;
}
}
return _50;
};
function _40(_57,_58){
var _59=false,_5a=false;
if(_57<_58){
_59=true;
}
if(_58>0){
_5a=true;
}
return {inc:_59,pos:_5a};
};
function _55(v){
var _5b;
if(v>-1&&v<1){
if(v<0){
if(v>=-_19){
_5b=-v;
}else{
_5b=v/Math.ceil(v/_19);
}
}else{
_5b=_19;
}
}else{
_5b=Math.abs(v)*_19;
}
return v+_5b;
};
function _41(p1,p2){
return (p2[y]-p1[y])/(p2[x]-p1[x]);
};
},Grapher:_1d});
});
