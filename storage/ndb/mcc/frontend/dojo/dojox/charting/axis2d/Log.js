//>>built
define("dojox/charting/axis2d/Log",["dojo/_base/lang","dojo/_base/array","dojo/_base/sniff","dojo/_base/declare","dojo/_base/connect","dojo/dom-geometry","./Invisible","../scaler/common","../scaler/linear","../scaler/log","./common","dojox/gfx","dojox/lang/utils","dojox/lang/functional"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,g,du,df){
var _c=45;
return _4("dojox.charting.axis2d.Log",_7,{defaultParams:{vertical:false,fixUpper:"none",fixLower:"none",natural:false,leftBottom:true,includeZero:false,fixed:true,majorLabels:true,minorTicks:true,minorLabels:true,microTicks:false,rotation:0,htmlLabels:true,enableCache:false,dropLabels:true,labelSizeChange:false,log:10},optionalParams:{min:0,max:1,from:0,to:1,majorTickStep:4,minorTickStep:2,microTickStep:1,labels:[],labelFunc:null,maxLabelSize:0,maxLabelCharCount:0,trailingSymbol:null,stroke:{},majorTick:{},minorTick:{},microTick:{},tick:{},font:"",fontColor:"",title:"",titleGap:0,titleFont:"",titleFontColor:"",titleOrientation:""},constructor:function(_d,_e){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_e);
du.updateWithPattern(this.opt,_e,this.optionalParams);
if(this.opt.enableCache){
this._textFreePool=[];
this._lineFreePool=[];
this._textUsePool=[];
this._lineUsePool=[];
}
this._invalidMaxLabelSize=true;
if(this.opt.log>1){
this.scalerType=_a;
this.scalerType.setBase(this.opt.log);
}else{
this.scalerType=_9;
}
},setWindow:function(_f,_10){
if(_f!=this.scale){
this._invalidMaxLabelSize=true;
}
return this.inherited(arguments);
},_groupLabelWidth:function(_11,_12,_13){
if(!_11.length){
return 0;
}
if(_11.length>50){
_11.length=50;
}
if(_1.isObject(_11[0])){
_11=df.map(_11,function(_14){
return _14.text;
});
}
if(_13){
_11=df.map(_11,function(_15){
return _1.trim(_15).length==0?"":_15.substring(0,_13)+this.trailingSymbol;
},this);
}
var s=_11.join("<br>");
return g._base._getTextBox(s,{font:_12}).w||0;
},_getMaxLabelSize:function(min,max,_16,_17,_18,_19){
if(this._maxLabelSize==null&&arguments.length==6){
var o=this.opt;
this.scaler.minMinorStep=this._prevMinMinorStep=0;
var ob=_1.clone(o);
delete ob.to;
delete ob.from;
var sb=this.scalerType.buildScaler(min,max,_16,ob,o.to-o.from);
sb.minMinorStep=0;
this._majorStart=sb.major.start;
var tb=this.scalerType.buildTicks(sb,o);
if(_19&&tb){
var _1a=0,_1b=0;
var _1c=function(_1d){
if(_1d.label){
this.push(_1d.label);
}
};
var _1e=[];
if(this.opt.majorLabels){
_2.forEach(tb.major,_1c,_1e);
_1a=this._groupLabelWidth(_1e,_18,ob.maxLabelCharCount);
if(ob.maxLabelSize){
_1a=Math.min(ob.maxLabelSize,_1a);
}
}
_1e=[];
if(this.opt.dropLabels&&this.opt.minorLabels){
_2.forEach(tb.minor,_1c,_1e);
_1b=this._groupLabelWidth(_1e,_18,ob.maxLabelCharCount);
if(ob.maxLabelSize){
_1b=Math.min(ob.maxLabelSize,_1b);
}
}
this._maxLabelSize={majLabelW:_1a,minLabelW:_1b,majLabelH:_19,minLabelH:_19};
}else{
this._maxLabelSize=null;
}
}
return this._maxLabelSize;
},calculate:function(min,max,_1f){
this.inherited(arguments,[min,max,_1f,this.scalerType]);
this.scaler.minMinorStep=this._prevMinMinorStep;
if((this._invalidMaxLabelSize||_1f!=this._oldSpan)&&(min!=Infinity&&max!=-Infinity)){
this._invalidMaxLabelSize=false;
if(this.opt.labelSizeChange){
this._maxLabelSize=null;
}
this._oldSpan=_1f;
var o=this.opt;
var ta=this.chart.theme.axis,_20=o.rotation%360,_21=this.chart.theme.axis.tick.labelGap,_22=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font),_23=_22?g.normalizedLength(g.splitFontString(_22).size):0,_24=this._getMaxLabelSize(min,max,_1f,_20,_22,_23);
if(typeof _21!="number"){
_21=4;
}
if(_24&&o.dropLabels){
var _25=Math.abs(Math.cos(_20*Math.PI/180)),_26=Math.abs(Math.sin(_20*Math.PI/180));
var _27,_28;
if(_20<0){
_20+=360;
}
switch(_20){
case 0:
case 180:
if(this.vertical){
_27=_28=_23;
}else{
_27=_24.majLabelW;
_28=_24.minLabelW;
}
break;
case 90:
case 270:
if(this.vertical){
_27=_24.majLabelW;
_28=_24.minLabelW;
}else{
_27=_28=_23;
}
break;
default:
_27=this.vertical?Math.min(_24.majLabelW,_23/_25):Math.min(_24.majLabelW,_23/_26);
var _29=Math.sqrt(_24.minLabelW*_24.minLabelW+_23*_23),_2a=this.vertical?_23*_25+_24.minLabelW*_26:_24.minLabelW*_25+_23*_26;
_28=Math.min(_29,_2a);
break;
}
this.scaler.minMinorStep=this._prevMinMinorStep=Math.max(_27,_28)+_21;
var _2b=this.scaler.minMinorStep<=this.scaler.minor.tick*this.scaler.bounds.scale;
if(!_2b){
this._skipInterval=Math.floor((_27+_21)/(this.scaler.major.tick*this.scaler.bounds.scale));
}else{
this._skipInterval=0;
}
}else{
this._skipInterval=0;
}
}
this.ticks=this.scalerType.buildTicks(this.scaler,this.opt);
return this;
},getOffsets:function(){
var s=this.scaler,_2c={l:0,r:0,t:0,b:0};
if(!s){
return _2c;
}
var o=this.opt,a,b,c,d,gl=_8.getNumericLabel,_2d=0,ma=s.major,mi=s.minor,ta=this.chart.theme.axis,_2e=this.chart.theme.axis.tick.labelGap,_2f=o.titleFont||(ta.title&&ta.title.font),_30=(o.titleGap==0)?0:o.titleGap||(ta.title&&ta.title.gap),_31=this.chart.theme.getTick("major",o),_32=this.chart.theme.getTick("minor",o),_33=_2f?g.normalizedLength(g.splitFontString(_2f).size):0,_34=o.rotation%360,_35=o.leftBottom,_36=Math.abs(Math.cos(_34*Math.PI/180)),_37=Math.abs(Math.sin(_34*Math.PI/180));
this.trailingSymbol=(o.trailingSymbol===undefined||o.trailingSymbol===null)?this.trailingSymbol:o.trailingSymbol;
if(typeof _2e!="number"){
_2e=4;
}
if(_34<0){
_34+=360;
}
var _38=this._getMaxLabelSize();
if(_38){
var _39;
var _3a=Math.ceil(Math.max(_38.majLabelW,_38.minLabelW))+1,_3b=Math.ceil(Math.max(_38.majLabelH,_38.minLabelH))+1;
if(this.vertical){
_39=_35?"l":"r";
switch(_34){
case 0:
case 180:
_2c[_39]=_3a;
_2c.t=_2c.b=_3b/2;
break;
case 90:
case 270:
_2c[_39]=_3b;
_2c.t=_2c.b=_3a/2;
break;
default:
if(_34<=_c||(180<_34&&_34<=(180+_c))){
_2c[_39]=_3b*_37/2+_3a*_36;
_2c[_35?"t":"b"]=_3b*_36/2+_3a*_37;
_2c[_35?"b":"t"]=_3b*_36/2;
}else{
if(_34>(360-_c)||(180>_34&&_34>(180-_c))){
_2c[_39]=_3b*_37/2+_3a*_36;
_2c[_35?"b":"t"]=_3b*_36/2+_3a*_37;
_2c[_35?"t":"b"]=_3b*_36/2;
}else{
if(_34<90||(180<_34&&_34<270)){
_2c[_39]=_3b*_37+_3a*_36;
_2c[_35?"t":"b"]=_3b*_36+_3a*_37;
}else{
_2c[_39]=_3b*_37+_3a*_36;
_2c[_35?"b":"t"]=_3b*_36+_3a*_37;
}
}
}
break;
}
_2c[_39]+=_2e+Math.max(_31.length,_32.length)+(o.title?(_33+_30):0);
}else{
_39=_35?"b":"t";
switch(_34){
case 0:
case 180:
_2c[_39]=_3b;
_2c.l=_2c.r=_3a/2;
break;
case 90:
case 270:
_2c[_39]=_3a;
_2c.l=_2c.r=_3b/2;
break;
default:
if((90-_c)<=_34&&_34<=90||(270-_c)<=_34&&_34<=270){
_2c[_39]=_3b*_36/2+_3a*_37;
_2c[_35?"r":"l"]=_3b*_37/2+_3a*_36;
_2c[_35?"l":"r"]=_3b*_37/2;
}else{
if(90<=_34&&_34<=(90+_c)||270<=_34&&_34<=(270+_c)){
_2c[_39]=_3b*_36/2+_3a*_37;
_2c[_35?"l":"r"]=_3b*_37/2+_3a*_36;
_2c[_35?"r":"l"]=_3b*_37/2;
}else{
if(_34<_c||(180<_34&&_34<(180+_c))){
_2c[_39]=_3b*_36+_3a*_37;
_2c[_35?"r":"l"]=_3b*_37+_3a*_36;
}else{
_2c[_39]=_3b*_36+_3a*_37;
_2c[_35?"l":"r"]=_3b*_37+_3a*_36;
}
}
}
break;
}
_2c[_39]+=_2e+Math.max(_31.length,_32.length)+(o.title?(_33+_30):0);
}
}
return _2c;
},cleanGroup:function(_3c){
if(this.opt.enableCache&&this.group){
this._lineFreePool=this._lineFreePool.concat(this._lineUsePool);
this._lineUsePool=[];
this._textFreePool=this._textFreePool.concat(this._textUsePool);
this._textUsePool=[];
}
this.inherited(arguments);
},createText:function(_3d,_3e,x,y,_3f,_40,_41,_42,_43){
if(!this.opt.enableCache||_3d=="html"){
return _b.createText[_3d](this.chart,_3e,x,y,_3f,_40,_41,_42,_43);
}
var _44;
if(this._textFreePool.length>0){
_44=this._textFreePool.pop();
_44.setShape({x:x,y:y,text:_40,align:_3f});
_3e.add(_44);
}else{
_44=_b.createText[_3d](this.chart,_3e,x,y,_3f,_40,_41,_42);
}
this._textUsePool.push(_44);
return _44;
},createLine:function(_45,_46){
var _47;
if(this.opt.enableCache&&this._lineFreePool.length>0){
_47=this._lineFreePool.pop();
_47.setShape(_46);
_45.add(_47);
}else{
_47=_45.createLine(_46);
}
if(this.opt.enableCache){
this._lineUsePool.push(_47);
}
return _47;
},render:function(dim,_48){
if(!this.dirty||!this.scaler){
return this;
}
var o=this.opt,ta=this.chart.theme.axis,_49=o.leftBottom,_4a=o.rotation%360,_4b,_4c,_4d,_4e=0,_4f,_50,_51,_52,_53,_54,_55=this.chart.theme.axis.tick.labelGap,_56=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font),_57=o.titleFont||(ta.title&&ta.title.font),_58=o.fontColor||(ta.majorTick&&ta.majorTick.fontColor)||(ta.tick&&ta.tick.fontColor)||"black",_59=o.titleFontColor||(ta.title&&ta.title.fontColor)||"black",_5a=(o.titleGap==0)?0:o.titleGap||(ta.title&&ta.title.gap)||15,_5b=o.titleOrientation||(ta.title&&ta.title.orientation)||"axis",_5c=this.chart.theme.getTick("major",o),_5d=this.chart.theme.getTick("minor",o),_5e=this.chart.theme.getTick("micro",o),_5f=Math.max(_5c.length,_5d.length,_5e.length),_60="stroke" in o?o.stroke:ta.stroke,_61=_56?g.normalizedLength(g.splitFontString(_56).size):0,_62=Math.abs(Math.cos(_4a*Math.PI/180)),_63=Math.abs(Math.sin(_4a*Math.PI/180)),_64=_57?g.normalizedLength(g.splitFontString(_57).size):0;
if(typeof _55!="number"){
_55=4;
}
if(_4a<0){
_4a+=360;
}
var _65=this._getMaxLabelSize();
_65=_65&&_65.majLabelW;
if(this.vertical){
_4b={y:dim.height-_48.b};
_4c={y:_48.t};
_4d={y:(dim.height-_48.b+_48.t)/2};
_4f=_61*_63+(_65||0)*_62+_55+Math.max(_5c.length,_5d.length)+_64+_5a;
_50={x:0,y:-1};
_53={x:0,y:0};
_51={x:1,y:0};
_52={x:_55,y:0};
switch(_4a){
case 0:
_54="end";
_53.y=_61*0.4;
break;
case 90:
_54="middle";
_53.x=-_61;
break;
case 180:
_54="start";
_53.y=-_61*0.4;
break;
case 270:
_54="middle";
break;
default:
if(_4a<_c){
_54="end";
_53.y=_61*0.4;
}else{
if(_4a<90){
_54="end";
_53.y=_61*0.4;
}else{
if(_4a<(180-_c)){
_54="start";
}else{
if(_4a<(180+_c)){
_54="start";
_53.y=-_61*0.4;
}else{
if(_4a<270){
_54="start";
_53.x=_49?0:_61*0.4;
}else{
if(_4a<(360-_c)){
_54="end";
_53.x=_49?0:_61*0.4;
}else{
_54="end";
_53.y=_61*0.4;
}
}
}
}
}
}
}
if(_49){
_4b.x=_4c.x=_48.l;
_4e=(_5b&&_5b=="away")?90:270;
_4d.x=_48.l-_4f+(_4e==270?_64:0);
_51.x=-1;
_52.x=-_52.x;
}else{
_4b.x=_4c.x=dim.width-_48.r;
_4e=(_5b&&_5b=="axis")?90:270;
_4d.x=dim.width-_48.r+_4f-(_4e==270?0:_64);
switch(_54){
case "start":
_54="end";
break;
case "end":
_54="start";
break;
case "middle":
_53.x+=_61;
break;
}
}
}else{
_4b={x:_48.l};
_4c={x:dim.width-_48.r};
_4d={x:(dim.width-_48.r+_48.l)/2};
_4f=_61*_62+(_65||0)*_63+_55+Math.max(_5c.length,_5d.length)+_64+_5a;
_50={x:1,y:0};
_53={x:0,y:0};
_51={x:0,y:1};
_52={x:0,y:_55};
switch(_4a){
case 0:
_54="middle";
_53.y=_61;
break;
case 90:
_54="start";
_53.x=-_61*0.4;
break;
case 180:
_54="middle";
break;
case 270:
_54="end";
_53.x=_61*0.4;
break;
default:
if(_4a<(90-_c)){
_54="start";
_53.y=_49?_61:0;
}else{
if(_4a<(90+_c)){
_54="start";
_53.x=-_61*0.4;
}else{
if(_4a<180){
_54="start";
_53.y=_49?0:-_61;
}else{
if(_4a<(270-_c)){
_54="end";
_53.y=_49?0:-_61;
}else{
if(_4a<(270+_c)){
_54="end";
_53.y=_49?_61*0.4:0;
}else{
_54="end";
_53.y=_49?_61:0;
}
}
}
}
}
}
if(_49){
_4b.y=_4c.y=dim.height-_48.b;
_4e=(_5b&&_5b=="axis")?180:0;
_4d.y=dim.height-_48.b+_4f-(_4e?_64:0);
}else{
_4b.y=_4c.y=_48.t;
_4e=(_5b&&_5b=="away")?180:0;
_4d.y=_48.t-_4f+(_4e?0:_64);
_51.y=-1;
_52.y=-_52.y;
switch(_54){
case "start":
_54="end";
break;
case "end":
_54="start";
break;
case "middle":
_53.y-=_61;
break;
}
}
}
this.cleanGroup();
var s=this.group,c=this.scaler,t=this.ticks,f=this.scalerType.getTransformerFromModel(this.scaler),_66=(!o.title||!_4e)&&!_4a&&this.opt.htmlLabels&&!_3("ie")&&!_3("opera")?"html":"gfx",dx=_51.x*_5c.length,dy=_51.y*_5c.length,_67=this._skipInterval;
s.createLine({x1:_4b.x,y1:_4b.y,x2:_4c.x,y2:_4c.y}).setStroke(_60);
if(o.title){
var _68=_b.createText[_66](this.chart,s,_4d.x,_4d.y,"middle",o.title,_57,_59);
if(_66=="html"){
this.htmlElements.push(_68);
}else{
_68.setTransform(g.matrix.rotategAt(_4e,_4d.x,_4d.y));
}
}
if(t==null){
this.dirty=false;
return this;
}
var _69=this.opt.majorLabels;
_2.forEach(t.major,function(_6a,i){
var _6b=f(_6a.value),_6c,x=_4b.x+_50.x*_6b,y=_4b.y+_50.y*_6b;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_5c);
if(_6a.label&&(!_67||(i-(1+_67))%(1+_67)==0)){
var _6d=o.maxLabelCharCount?this.getTextWithLimitCharCount(_6a.label,_56,o.maxLabelCharCount):{text:_6a.label,truncated:false};
_6d=o.maxLabelSize?this.getTextWithLimitLength(_6d.text,_56,o.maxLabelSize,_6d.truncated):_6d;
_6c=this.createText(_66,s,x+dx+_52.x+(_4a?0:_53.x),y+dy+_52.y+(_4a?0:_53.y),_54,_6d.text,_56,_58);
if(this.chart.truncateBidi&&_6d.truncated){
this.chart.truncateBidi(_6c,_6a.label,_66);
}
_6d.truncated&&this.labelTooltip(_6c,this.chart,_6a.label,_6d.text,_56,_66);
if(_66=="html"){
this.htmlElements.push(_6c);
}else{
if(_4a){
_6c.setTransform([{dx:_53.x,dy:_53.y},g.matrix.rotategAt(_4a,x+dx+_52.x,y+dy+_52.y)]);
}
}
}
},this);
dx=_51.x*_5d.length;
dy=_51.y*_5d.length;
_69=this.opt.minorLabels&&!_67&&this.opt.log===10&&t.minor.length;
if(_69){
var _6e=1,_6f=Math.log(10);
_2.forEach(t.minor,function(_70,i){
var _71=Math.log(_70.value)/_6f,_72=Math.floor(_71),_73=Math.ceil(_71);
_6e=Math.min(_6e,_71-_72,_73-_71);
if(i){
_6e=Math.min(_6e,_71-Math.log(t.minor[i-1].value)/_6f);
}
});
_69=c.minMinorStep<=_6e*c.bounds.scale;
}
_2.forEach(t.minor,function(_74){
var _75=f(_74.value),_76,x=_4b.x+_50.x*_75,y=_4b.y+_50.y*_75;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_5d);
if(_69&&_74.label){
var _77=o.maxLabelCharCount?this.getTextWithLimitCharCount(_74.label,_56,o.maxLabelCharCount):{text:_74.label,truncated:false};
_77=o.maxLabelSize?this.getTextWithLimitLength(_77.text,_56,o.maxLabelSize,_77.truncated):_77;
_76=this.createText(_66,s,x+dx+_52.x+(_4a?0:_53.x),y+dy+_52.y+(_4a?0:_53.y),_54,_77.text,_56,_58);
if(this.chart.getTextDir&&_77.truncated){
this.chart.truncateBidi(_76,_74.label,_66);
}
_77.truncated&&this.labelTooltip(_76,this.chart,_74.label,_77.text,_56,_66);
if(_66=="html"){
this.htmlElements.push(_76);
}else{
if(_4a){
_76.setTransform([{dx:_53.x,dy:_53.y},g.matrix.rotategAt(_4a,x+dx+_52.x,y+dy+_52.y)]);
}
}
}
},this);
dx=_51.x*_5e.length;
dy=_51.y*_5e.length;
_2.forEach(t.micro,function(_78){
var _79=f(_78.value),_7a,x=_4b.x+_50.x*_79,y=_4b.y+_50.y*_79;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_5e);
},this);
this.dirty=false;
return this;
},labelTooltip:function(_7b,_7c,_7d,_7e,_7f,_80){
var _81=["dijit/Tooltip"];
var _82={type:"rect"},_83=["above","below"],_84=g._base._getTextBox(_7e,{font:_7f}).w||0,_85=_7f?g.normalizedLength(g.splitFontString(_7f).size):0;
if(_80=="html"){
_1.mixin(_82,_6.position(_7b.firstChild,true));
_82.width=Math.ceil(_84);
_82.height=Math.ceil(_85);
this._events.push({shape:dojo,handle:_5.connect(_7b.firstChild,"onmouseover",this,function(e){
require(_81,function(_86){
_86.show(_7d,_82,_83);
});
})});
this._events.push({shape:dojo,handle:_5.connect(_7b.firstChild,"onmouseout",this,function(e){
require(_81,function(_87){
_87.hide(_82);
});
})});
}else{
var shp=_7b.getShape(),lt=_7c.getCoords();
_82=_1.mixin(_82,{x:shp.x-_84/2,y:shp.y});
_82.x+=lt.x;
_82.y+=lt.y;
_82.x=Math.round(_82.x);
_82.y=Math.round(_82.y);
_82.width=Math.ceil(_84);
_82.height=Math.ceil(_85);
this._events.push({shape:_7b,handle:_7b.connect("onmouseenter",this,function(e){
require(_81,function(_88){
_88.show(_7d,_82,_83);
});
})});
this._events.push({shape:_7b,handle:_7b.connect("onmouseleave",this,function(e){
require(_81,function(_89){
_89.hide(_82);
});
})});
}
},isNullValue:function(_8a){
return _8a<=0;
},naturalBaseline:1});
});
