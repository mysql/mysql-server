//>>built
define("dojox/charting/axis2d/Default",["dojo/_base/lang","dojo/_base/array","dojo/_base/sniff","dojo/_base/declare","dojo/_base/connect","dojo/dom-geometry","./Invisible","../scaler/common","../scaler/linear","./common","dojox/gfx","dojox/lang/utils","dojox/lang/functional"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,g,du,df){
var _b=45;
return _4("dojox.charting.axis2d.Default",_7,{defaultParams:{vertical:false,fixUpper:"none",fixLower:"none",natural:false,leftBottom:true,includeZero:false,fixed:true,majorLabels:true,minorTicks:true,minorLabels:true,microTicks:false,rotation:0,htmlLabels:true,enableCache:false,dropLabels:true,labelSizeChange:false},optionalParams:{min:0,max:1,from:0,to:1,majorTickStep:4,minorTickStep:2,microTickStep:1,labels:[],labelFunc:null,maxLabelSize:0,maxLabelCharCount:0,trailingSymbol:null,stroke:{},majorTick:{},minorTick:{},microTick:{},tick:{},font:"",fontColor:"",title:"",titleGap:0,titleFont:"",titleFontColor:"",titleOrientation:""},constructor:function(_c,_d){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_d);
du.updateWithPattern(this.opt,_d,this.optionalParams);
if(this.opt.enableCache){
this._textFreePool=[];
this._lineFreePool=[];
this._textUsePool=[];
this._lineUsePool=[];
}
this._invalidMaxLabelSize=true;
},setWindow:function(_e,_f){
if(_e!=this.scale){
this._invalidMaxLabelSize=true;
}
return this.inherited(arguments);
},_groupLabelWidth:function(_10,_11,_12){
if(!_10.length){
return 0;
}
if(_10.length>50){
_10.length=50;
}
if(_1.isObject(_10[0])){
_10=df.map(_10,function(_13){
return _13.text;
});
}
if(_12){
_10=df.map(_10,function(_14){
return _1.trim(_14).length==0?"":_14.substring(0,_12)+this.trailingSymbol;
},this);
}
var s=_10.join("<br>");
return g._base._getTextBox(s,{font:_11}).w||0;
},_getMaxLabelSize:function(min,max,_15,_16,_17,_18){
if(this._maxLabelSize==null&&arguments.length==6){
var o=this.opt;
this.scaler.minMinorStep=this._prevMinMinorStep=0;
var ob=_1.clone(o);
delete ob.to;
delete ob.from;
var sb=_9.buildScaler(min,max,_15,ob,o.to-o.from);
sb.minMinorStep=0;
this._majorStart=sb.major.start;
var tb=_9.buildTicks(sb,o);
if(_18&&tb){
var _19=0,_1a=0;
var _1b=function(_1c){
if(_1c.label){
this.push(_1c.label);
}
};
var _1d=[];
if(this.opt.majorLabels){
_2.forEach(tb.major,_1b,_1d);
_19=this._groupLabelWidth(_1d,_17,ob.maxLabelCharCount);
if(ob.maxLabelSize){
_19=Math.min(ob.maxLabelSize,_19);
}
}
_1d=[];
if(this.opt.dropLabels&&this.opt.minorLabels){
_2.forEach(tb.minor,_1b,_1d);
_1a=this._groupLabelWidth(_1d,_17,ob.maxLabelCharCount);
if(ob.maxLabelSize){
_1a=Math.min(ob.maxLabelSize,_1a);
}
}
this._maxLabelSize={majLabelW:_19,minLabelW:_1a,majLabelH:_18,minLabelH:_18};
}else{
this._maxLabelSize=null;
}
}
return this._maxLabelSize;
},calculate:function(min,max,_1e){
this.inherited(arguments);
this.scaler.minMinorStep=this._prevMinMinorStep;
if((this._invalidMaxLabelSize||_1e!=this._oldSpan)&&(min!=Infinity&&max!=-Infinity)){
this._invalidMaxLabelSize=false;
if(this.opt.labelSizeChange){
this._maxLabelSize=null;
}
this._oldSpan=_1e;
var o=this.opt;
var ta=this.chart.theme.axis,_1f=o.rotation%360,_20=this.chart.theme.axis.tick.labelGap,_21=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font),_22=_21?g.normalizedLength(g.splitFontString(_21).size):0,_23=this._getMaxLabelSize(min,max,_1e,_1f,_21,_22);
if(typeof _20!="number"){
_20=4;
}
if(_23&&o.dropLabels){
var _24=Math.abs(Math.cos(_1f*Math.PI/180)),_25=Math.abs(Math.sin(_1f*Math.PI/180));
var _26,_27;
if(_1f<0){
_1f+=360;
}
switch(_1f){
case 0:
case 180:
if(this.vertical){
_26=_27=_22;
}else{
_26=_23.majLabelW;
_27=_23.minLabelW;
}
break;
case 90:
case 270:
if(this.vertical){
_26=_23.majLabelW;
_27=_23.minLabelW;
}else{
_26=_27=_22;
}
break;
default:
_26=this.vertical?Math.min(_23.majLabelW,_22/_24):Math.min(_23.majLabelW,_22/_25);
var _28=Math.sqrt(_23.minLabelW*_23.minLabelW+_22*_22),_29=this.vertical?_22*_24+_23.minLabelW*_25:_23.minLabelW*_24+_22*_25;
_27=Math.min(_28,_29);
break;
}
this.scaler.minMinorStep=this._prevMinMinorStep=Math.max(_26,_27)+_20;
var _2a=this.scaler.minMinorStep<=this.scaler.minor.tick*this.scaler.bounds.scale;
if(!_2a){
this._skipInterval=Math.floor((_26+_20)/(this.scaler.major.tick*this.scaler.bounds.scale));
}else{
this._skipInterval=0;
}
}else{
this._skipInterval=0;
}
}
this.ticks=_9.buildTicks(this.scaler,this.opt);
return this;
},getOffsets:function(){
var s=this.scaler,_2b={l:0,r:0,t:0,b:0};
if(!s){
return _2b;
}
var o=this.opt,a,b,c,d,gl=_8.getNumericLabel,_2c=0,ma=s.major,mi=s.minor,ta=this.chart.theme.axis,_2d=this.chart.theme.axis.tick.labelGap,_2e=o.titleFont||(ta.title&&ta.title.font),_2f=(o.titleGap==0)?0:o.titleGap||(ta.title&&ta.title.gap),_30=this.chart.theme.getTick("major",o),_31=this.chart.theme.getTick("minor",o),_32=_2e?g.normalizedLength(g.splitFontString(_2e).size):0,_33=o.rotation%360,_34=o.leftBottom,_35=Math.abs(Math.cos(_33*Math.PI/180)),_36=Math.abs(Math.sin(_33*Math.PI/180));
this.trailingSymbol=(o.trailingSymbol===undefined||o.trailingSymbol===null)?this.trailingSymbol:o.trailingSymbol;
if(typeof _2d!="number"){
_2d=4;
}
if(_33<0){
_33+=360;
}
var _37=this._getMaxLabelSize();
if(_37){
var _38;
var _39=Math.ceil(Math.max(_37.majLabelW,_37.minLabelW))+1,_3a=Math.ceil(Math.max(_37.majLabelH,_37.minLabelH))+1;
if(this.vertical){
_38=_34?"l":"r";
switch(_33){
case 0:
case 180:
_2b[_38]=_39;
_2b.t=_2b.b=_3a/2;
break;
case 90:
case 270:
_2b[_38]=_3a;
_2b.t=_2b.b=_39/2;
break;
default:
if(_33<=_b||(180<_33&&_33<=(180+_b))){
_2b[_38]=_3a*_36/2+_39*_35;
_2b[_34?"t":"b"]=_3a*_35/2+_39*_36;
_2b[_34?"b":"t"]=_3a*_35/2;
}else{
if(_33>(360-_b)||(180>_33&&_33>(180-_b))){
_2b[_38]=_3a*_36/2+_39*_35;
_2b[_34?"b":"t"]=_3a*_35/2+_39*_36;
_2b[_34?"t":"b"]=_3a*_35/2;
}else{
if(_33<90||(180<_33&&_33<270)){
_2b[_38]=_3a*_36+_39*_35;
_2b[_34?"t":"b"]=_3a*_35+_39*_36;
}else{
_2b[_38]=_3a*_36+_39*_35;
_2b[_34?"b":"t"]=_3a*_35+_39*_36;
}
}
}
break;
}
_2b[_38]+=_2d+Math.max(_30.length,_31.length)+(o.title?(_32+_2f):0);
}else{
_38=_34?"b":"t";
switch(_33){
case 0:
case 180:
_2b[_38]=_3a;
_2b.l=_2b.r=_39/2;
break;
case 90:
case 270:
_2b[_38]=_39;
_2b.l=_2b.r=_3a/2;
break;
default:
if((90-_b)<=_33&&_33<=90||(270-_b)<=_33&&_33<=270){
_2b[_38]=_3a*_35/2+_39*_36;
_2b[_34?"r":"l"]=_3a*_36/2+_39*_35;
_2b[_34?"l":"r"]=_3a*_36/2;
}else{
if(90<=_33&&_33<=(90+_b)||270<=_33&&_33<=(270+_b)){
_2b[_38]=_3a*_35/2+_39*_36;
_2b[_34?"l":"r"]=_3a*_36/2+_39*_35;
_2b[_34?"r":"l"]=_3a*_36/2;
}else{
if(_33<_b||(180<_33&&_33<(180+_b))){
_2b[_38]=_3a*_35+_39*_36;
_2b[_34?"r":"l"]=_3a*_36+_39*_35;
}else{
_2b[_38]=_3a*_35+_39*_36;
_2b[_34?"l":"r"]=_3a*_36+_39*_35;
}
}
}
break;
}
_2b[_38]+=_2d+Math.max(_30.length,_31.length)+(o.title?(_32+_2f):0);
}
}
return _2b;
},cleanGroup:function(_3b){
if(this.opt.enableCache&&this.group){
this._lineFreePool=this._lineFreePool.concat(this._lineUsePool);
this._lineUsePool=[];
this._textFreePool=this._textFreePool.concat(this._textUsePool);
this._textUsePool=[];
}
this.inherited(arguments);
},createText:function(_3c,_3d,x,y,_3e,_3f,_40,_41,_42){
if(!this.opt.enableCache||_3c=="html"){
return _a.createText[_3c](this.chart,_3d,x,y,_3e,_3f,_40,_41,_42);
}
var _43;
if(this._textFreePool.length>0){
_43=this._textFreePool.pop();
_43.setShape({x:x,y:y,text:_3f,align:_3e});
_3d.add(_43);
}else{
_43=_a.createText[_3c](this.chart,_3d,x,y,_3e,_3f,_40,_41);
}
this._textUsePool.push(_43);
return _43;
},createLine:function(_44,_45){
var _46;
if(this.opt.enableCache&&this._lineFreePool.length>0){
_46=this._lineFreePool.pop();
_46.setShape(_45);
_44.add(_46);
}else{
_46=_44.createLine(_45);
}
if(this.opt.enableCache){
this._lineUsePool.push(_46);
}
return _46;
},render:function(dim,_47){
if(!this.dirty||!this.scaler){
return this;
}
var o=this.opt,ta=this.chart.theme.axis,_48=o.leftBottom,_49=o.rotation%360,_4a,_4b,_4c,_4d=0,_4e,_4f,_50,_51,_52,_53,_54=this.chart.theme.axis.tick.labelGap,_55=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font),_56=o.titleFont||(ta.title&&ta.title.font),_57=o.fontColor||(ta.majorTick&&ta.majorTick.fontColor)||(ta.tick&&ta.tick.fontColor)||"black",_58=o.titleFontColor||(ta.title&&ta.title.fontColor)||"black",_59=(o.titleGap==0)?0:o.titleGap||(ta.title&&ta.title.gap)||15,_5a=o.titleOrientation||(ta.title&&ta.title.orientation)||"axis",_5b=this.chart.theme.getTick("major",o),_5c=this.chart.theme.getTick("minor",o),_5d=this.chart.theme.getTick("micro",o),_5e=Math.max(_5b.length,_5c.length,_5d.length),_5f="stroke" in o?o.stroke:ta.stroke,_60=_55?g.normalizedLength(g.splitFontString(_55).size):0,_61=Math.abs(Math.cos(_49*Math.PI/180)),_62=Math.abs(Math.sin(_49*Math.PI/180)),_63=_56?g.normalizedLength(g.splitFontString(_56).size):0;
if(typeof _54!="number"){
_54=4;
}
if(_49<0){
_49+=360;
}
var _64=this._getMaxLabelSize();
_64=_64&&_64.majLabelW;
if(this.vertical){
_4a={y:dim.height-_47.b};
_4b={y:_47.t};
_4c={y:(dim.height-_47.b+_47.t)/2};
_4e=_60*_62+(_64||0)*_61+_54+Math.max(_5b.length,_5c.length)+_63+_59;
_4f={x:0,y:-1};
_52={x:0,y:0};
_50={x:1,y:0};
_51={x:_54,y:0};
switch(_49){
case 0:
_53="end";
_52.y=_60*0.4;
break;
case 90:
_53="middle";
_52.x=-_60;
break;
case 180:
_53="start";
_52.y=-_60*0.4;
break;
case 270:
_53="middle";
break;
default:
if(_49<_b){
_53="end";
_52.y=_60*0.4;
}else{
if(_49<90){
_53="end";
_52.y=_60*0.4;
}else{
if(_49<(180-_b)){
_53="start";
}else{
if(_49<(180+_b)){
_53="start";
_52.y=-_60*0.4;
}else{
if(_49<270){
_53="start";
_52.x=_48?0:_60*0.4;
}else{
if(_49<(360-_b)){
_53="end";
_52.x=_48?0:_60*0.4;
}else{
_53="end";
_52.y=_60*0.4;
}
}
}
}
}
}
}
if(_48){
_4a.x=_4b.x=_47.l;
_4d=(_5a&&_5a=="away")?90:270;
_4c.x=_47.l-_4e+(_4d==270?_63:0);
_50.x=-1;
_51.x=-_51.x;
}else{
_4a.x=_4b.x=dim.width-_47.r;
_4d=(_5a&&_5a=="axis")?90:270;
_4c.x=dim.width-_47.r+_4e-(_4d==270?0:_63);
switch(_53){
case "start":
_53="end";
break;
case "end":
_53="start";
break;
case "middle":
_52.x+=_60;
break;
}
}
}else{
_4a={x:_47.l};
_4b={x:dim.width-_47.r};
_4c={x:(dim.width-_47.r+_47.l)/2};
_4e=_60*_61+(_64||0)*_62+_54+Math.max(_5b.length,_5c.length)+_63+_59;
_4f={x:1,y:0};
_52={x:0,y:0};
_50={x:0,y:1};
_51={x:0,y:_54};
switch(_49){
case 0:
_53="middle";
_52.y=_60;
break;
case 90:
_53="start";
_52.x=-_60*0.4;
break;
case 180:
_53="middle";
break;
case 270:
_53="end";
_52.x=_60*0.4;
break;
default:
if(_49<(90-_b)){
_53="start";
_52.y=_48?_60:0;
}else{
if(_49<(90+_b)){
_53="start";
_52.x=-_60*0.4;
}else{
if(_49<180){
_53="start";
_52.y=_48?0:-_60;
}else{
if(_49<(270-_b)){
_53="end";
_52.y=_48?0:-_60;
}else{
if(_49<(270+_b)){
_53="end";
_52.y=_48?_60*0.4:0;
}else{
_53="end";
_52.y=_48?_60:0;
}
}
}
}
}
}
if(_48){
_4a.y=_4b.y=dim.height-_47.b;
_4d=(_5a&&_5a=="axis")?180:0;
_4c.y=dim.height-_47.b+_4e-(_4d?_63:0);
}else{
_4a.y=_4b.y=_47.t;
_4d=(_5a&&_5a=="away")?180:0;
_4c.y=_47.t-_4e+(_4d?0:_63);
_50.y=-1;
_51.y=-_51.y;
switch(_53){
case "start":
_53="end";
break;
case "end":
_53="start";
break;
case "middle":
_52.y-=_60;
break;
}
}
}
this.cleanGroup();
var s=this.group,c=this.scaler,t=this.ticks,f=_9.getTransformerFromModel(this.scaler),_65=(!o.title||!_4d)&&!_49&&this.opt.htmlLabels&&!_3("ie")&&!_3("opera")?"html":"gfx",dx=_50.x*_5b.length,dy=_50.y*_5b.length,_66=this._skipInterval;
s.createLine({x1:_4a.x,y1:_4a.y,x2:_4b.x,y2:_4b.y}).setStroke(_5f);
if(o.title){
var _67=_a.createText[_65](this.chart,s,_4c.x,_4c.y,"middle",o.title,_56,_58);
if(_65=="html"){
this.htmlElements.push(_67);
}else{
_67.setTransform(g.matrix.rotategAt(_4d,_4c.x,_4c.y));
}
}
if(t==null){
this.dirty=false;
return this;
}
var rel=(t.major.length>0)?(t.major[0].value-this._majorStart)/c.major.tick:0;
var _68=this.opt.majorLabels;
_2.forEach(t.major,function(_69,i){
var _6a=f(_69.value),_6b,x=_4a.x+_4f.x*_6a,y=_4a.y+_4f.y*_6a;
i+=rel;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_5b);
if(_69.label&&(!_66||(i-(1+_66))%(1+_66)==0)){
var _6c=o.maxLabelCharCount?this.getTextWithLimitCharCount(_69.label,_55,o.maxLabelCharCount):{text:_69.label,truncated:false};
_6c=o.maxLabelSize?this.getTextWithLimitLength(_6c.text,_55,o.maxLabelSize,_6c.truncated):_6c;
_6b=this.createText(_65,s,x+dx+_51.x+(_49?0:_52.x),y+dy+_51.y+(_49?0:_52.y),_53,_6c.text,_55,_57);
if(this.chart.truncateBidi&&_6c.truncated){
this.chart.truncateBidi(_6b,_69.label,_65);
}
_6c.truncated&&this.labelTooltip(_6b,this.chart,_69.label,_6c.text,_55,_65);
if(_65=="html"){
this.htmlElements.push(_6b);
}else{
if(_49){
_6b.setTransform([{dx:_52.x,dy:_52.y},g.matrix.rotategAt(_49,x+dx+_51.x,y+dy+_51.y)]);
}
}
}
},this);
dx=_50.x*_5c.length;
dy=_50.y*_5c.length;
_68=this.opt.minorLabels&&c.minMinorStep<=c.minor.tick*c.bounds.scale;
_2.forEach(t.minor,function(_6d){
var _6e=f(_6d.value),_6f,x=_4a.x+_4f.x*_6e,y=_4a.y+_4f.y*_6e;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_5c);
if(_68&&_6d.label){
var _70=o.maxLabelCharCount?this.getTextWithLimitCharCount(_6d.label,_55,o.maxLabelCharCount):{text:_6d.label,truncated:false};
_70=o.maxLabelSize?this.getTextWithLimitLength(_70.text,_55,o.maxLabelSize,_70.truncated):_70;
_6f=this.createText(_65,s,x+dx+_51.x+(_49?0:_52.x),y+dy+_51.y+(_49?0:_52.y),_53,_70.text,_55,_57);
if(this.chart.getTextDir&&_70.truncated){
this.chart.truncateBidi(_6f,_6d.label,_65);
}
_70.truncated&&this.labelTooltip(_6f,this.chart,_6d.label,_70.text,_55,_65);
if(_65=="html"){
this.htmlElements.push(_6f);
}else{
if(_49){
_6f.setTransform([{dx:_52.x,dy:_52.y},g.matrix.rotategAt(_49,x+dx+_51.x,y+dy+_51.y)]);
}
}
}
},this);
dx=_50.x*_5d.length;
dy=_50.y*_5d.length;
_2.forEach(t.micro,function(_71){
var _72=f(_71.value),_73,x=_4a.x+_4f.x*_72,y=_4a.y+_4f.y*_72;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_5d);
},this);
this.dirty=false;
return this;
},labelTooltip:function(_74,_75,_76,_77,_78,_79){
var _7a=["dijit/Tooltip"];
var _7b={type:"rect"},_7c=["above","below"],_7d=g._base._getTextBox(_77,{font:_78}).w||0,_7e=_78?g.normalizedLength(g.splitFontString(_78).size):0;
if(_79=="html"){
_1.mixin(_7b,_6.position(_74.firstChild,true));
_7b.width=Math.ceil(_7d);
_7b.height=Math.ceil(_7e);
this._events.push({shape:dojo,handle:_5.connect(_74.firstChild,"onmouseover",this,function(e){
require(_7a,function(_7f){
_7f.show(_76,_7b,_7c);
});
})});
this._events.push({shape:dojo,handle:_5.connect(_74.firstChild,"onmouseout",this,function(e){
require(_7a,function(_80){
_80.hide(_7b);
});
})});
}else{
var shp=_74.getShape(),lt=_75.getCoords();
_7b=_1.mixin(_7b,{x:shp.x-_7d/2,y:shp.y});
_7b.x+=lt.x;
_7b.y+=lt.y;
_7b.x=Math.round(_7b.x);
_7b.y=Math.round(_7b.y);
_7b.width=Math.ceil(_7d);
_7b.height=Math.ceil(_7e);
this._events.push({shape:_74,handle:_74.connect("onmouseenter",this,function(e){
require(_7a,function(_81){
_81.show(_76,_7b,_7c);
});
})});
this._events.push({shape:_74,handle:_74.connect("onmouseleave",this,function(e){
require(_7a,function(_82){
_82.hide(_7b);
});
})});
}
}});
});
