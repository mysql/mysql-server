//>>built
define("dojox/charting/axis2d/Default",["dojo/_base/lang","dojo/_base/array","dojo/sniff","dojo/_base/declare","dojo/_base/connect","dojo/dom-geometry","./Invisible","../scaler/linear","./common","dojox/gfx","dojox/lang/utils","dojox/lang/functional","dojo/has!dojo-bidi?../bidi/axis2d/Default"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,g,du,df,_a){
var _b=45;
var _c=_4(_3("dojo-bidi")?"dojox.charting.axis2d.NonBidiDefault":"dojox.charting.axis2d.Default",_7,{defaultParams:{vertical:false,fixUpper:"none",fixLower:"none",natural:false,leftBottom:true,includeZero:false,fixed:true,majorLabels:true,minorTicks:true,minorLabels:true,microTicks:false,rotation:0,htmlLabels:true,enableCache:false,dropLabels:true,labelSizeChange:false,position:"leftOrBottom"},optionalParams:{min:0,max:1,from:0,to:1,majorTickStep:4,minorTickStep:2,microTickStep:1,labels:[],labelFunc:null,maxLabelSize:0,maxLabelCharCount:0,trailingSymbol:null,stroke:{},majorTick:{},minorTick:{},microTick:{},tick:{},font:"",fontColor:"",title:"",titleGap:0,titleFont:"",titleFontColor:"",titleOrientation:""},constructor:function(_d,_e){
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
if(!(_e&&("position" in _e))){
this.opt.position=this.opt.leftBottom?"leftOrBottom":"rightOrTop";
}
this.renderingOptions={"shape-rendering":"crispEdges"};
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
var sb=_8.buildScaler(min,max,_16,ob,o.to-o.from);
sb.minMinorStep=0;
this._majorStart=sb.major.start;
var tb=_8.buildTicks(sb,o);
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
this.inherited(arguments);
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
this.ticks=_8.buildTicks(this.scaler,this.opt);
return this;
},getOffsets:function(){
var s=this.scaler,_2c={l:0,r:0,t:0,b:0};
if(!s){
return _2c;
}
var o=this.opt,ta=this.chart.theme.axis,_2d=this.chart.theme.axis.tick.labelGap,_2e=o.titleFont||(ta.title&&ta.title.font),_2f=(o.titleGap==0)?0:o.titleGap||(ta.title&&ta.title.gap),_30=this.chart.theme.getTick("major",o),_31=this.chart.theme.getTick("minor",o),_32=_2e?g.normalizedLength(g.splitFontString(_2e).size):0,_33=o.rotation%360,_34=o.position,_35=_34!=="rightOrTop",_36=Math.abs(Math.cos(_33*Math.PI/180)),_37=Math.abs(Math.sin(_33*Math.PI/180));
this.trailingSymbol=(o.trailingSymbol===undefined||o.trailingSymbol===null)?this.trailingSymbol:o.trailingSymbol;
if(typeof _2d!="number"){
_2d=4;
}
if(_33<0){
_33+=360;
}
var _38=this._getMaxLabelSize();
if(_38){
var _39;
var _3a=Math.ceil(Math.max(_38.majLabelW,_38.minLabelW))+1,_3b=Math.ceil(Math.max(_38.majLabelH,_38.minLabelH))+1;
if(this.vertical){
_39=_35?"l":"r";
switch(_33){
case 0:
case 180:
_2c[_39]=_34==="center"?0:_3a;
_2c.t=_2c.b=_3b/2;
break;
case 90:
case 270:
_2c[_39]=_3b;
_2c.t=_2c.b=_3a/2;
break;
default:
if(_33<=_b||(180<_33&&_33<=(180+_b))){
_2c[_39]=_3b*_37/2+_3a*_36;
_2c[_35?"t":"b"]=_3b*_36/2+_3a*_37;
_2c[_35?"b":"t"]=_3b*_36/2;
}else{
if(_33>(360-_b)||(180>_33&&_33>(180-_b))){
_2c[_39]=_3b*_37/2+_3a*_36;
_2c[_35?"b":"t"]=_3b*_36/2+_3a*_37;
_2c[_35?"t":"b"]=_3b*_36/2;
}else{
if(_33<90||(180<_33&&_33<270)){
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
if(_34==="center"){
_2c[_39]=0;
}else{
_2c[_39]+=_2d+Math.max(_30.length>0?_30.length:0,_31.length>0?_31.length:0)+(o.title?(_32+_2f):0);
}
}else{
_39=_35?"b":"t";
switch(_33){
case 0:
case 180:
_2c[_39]=_34==="center"?0:_3b;
_2c.l=_2c.r=_3a/2;
break;
case 90:
case 270:
_2c[_39]=_3a;
_2c.l=_2c.r=_3b/2;
break;
default:
if((90-_b)<=_33&&_33<=90||(270-_b)<=_33&&_33<=270){
_2c[_39]=_3b*_36/2+_3a*_37;
_2c[_35?"r":"l"]=_3b*_37/2+_3a*_36;
_2c[_35?"l":"r"]=_3b*_37/2;
}else{
if(90<=_33&&_33<=(90+_b)||270<=_33&&_33<=(270+_b)){
_2c[_39]=_3b*_36/2+_3a*_37;
_2c[_35?"l":"r"]=_3b*_37/2+_3a*_36;
_2c[_35?"r":"l"]=_3b*_37/2;
}else{
if(_33<_b||(180<_33&&_33<(180+_b))){
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
if(_34==="center"){
_2c[_39]=0;
}else{
_2c[_39]+=_2d+Math.max(_30.length>0?_30.length:0,_31.length>0?_31.length:0)+(o.title?(_32+_2f):0);
}
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
return _9.createText[_3d](this.chart,_3e,x,y,_3f,_40,_41,_42,_43);
}
var _44;
if(this._textFreePool.length>0){
_44=this._textFreePool.pop();
_44.setShape({x:x,y:y,text:_40,align:_3f});
_3e.add(_44);
}else{
_44=_9.createText[_3d](this.chart,_3e,x,y,_3f,_40,_41,_42);
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
var _49=this._isRtl();
if(!this.dirty||!this.scaler){
return this;
}
var o=this.opt,ta=this.chart.theme.axis,_4a=o.position,_4b=_4a!=="rightOrTop",_4c=o.rotation%360,_4d,_4e,_4f,_50=0,_51,_52,_53,_54,_55,_56,_57=this.chart.theme.axis.tick.labelGap,_58=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font),_59=o.titleFont||(ta.title&&ta.title.font),_5a=o.fontColor||(ta.majorTick&&ta.majorTick.fontColor)||(ta.tick&&ta.tick.fontColor)||"black",_5b=o.titleFontColor||(ta.title&&ta.title.fontColor)||"black",_5c=(o.titleGap==0)?0:o.titleGap||(ta.title&&ta.title.gap)||15,_5d=o.titleOrientation||(ta.title&&ta.title.orientation)||"axis",_5e=this.chart.theme.getTick("major",o),_5f=this.chart.theme.getTick("minor",o),_60=this.chart.theme.getTick("micro",o),_61="stroke" in o?o.stroke:ta.stroke,_62=_58?g.normalizedLength(g.splitFontString(_58).size):0,_63=Math.abs(Math.cos(_4c*Math.PI/180)),_64=Math.abs(Math.sin(_4c*Math.PI/180)),_65=_59?g.normalizedLength(g.splitFontString(_59).size):0;
if(typeof _57!="number"){
_57=4;
}
if(_4c<0){
_4c+=360;
}
var _66=this._getMaxLabelSize();
_66=_66&&_66.majLabelW;
if(this.vertical){
_4d={y:dim.height-_48.b};
_4e={y:_48.t};
_4f={y:(dim.height-_48.b+_48.t)/2};
_51=_62*_64+(_66||0)*_63+_57+Math.max(_5e.length>0?_5e.length:0,_5f.length>0?_5f.length:0)+_65+_5c;
_52={x:0,y:-1};
_55={x:0,y:0};
_53={x:1,y:0};
_54={x:_57,y:0};
switch(_4c){
case 0:
_56="end";
_55.y=_62*0.4;
break;
case 90:
_56="middle";
_55.x=-_62;
break;
case 180:
_56="start";
_55.y=-_62*0.4;
break;
case 270:
_56="middle";
break;
default:
if(_4c<_b){
_56="end";
_55.y=_62*0.4;
}else{
if(_4c<90){
_56="end";
_55.y=_62*0.4;
}else{
if(_4c<(180-_b)){
_56="start";
}else{
if(_4c<(180+_b)){
_56="start";
_55.y=-_62*0.4;
}else{
if(_4c<270){
_56="start";
_55.x=_4b?0:_62*0.4;
}else{
if(_4c<(360-_b)){
_56="end";
_55.x=_4b?0:_62*0.4;
}else{
_56="end";
_55.y=_62*0.4;
}
}
}
}
}
}
}
if(_4b){
_4d.x=_4e.x=_4a==="center"?dim.width/2:_48.l;
_50=(_5d&&_5d=="away")?90:270;
_4f.x=_48.l-_51+(_50==270?_65:0);
_53.x=-1;
_54.x=-_54.x;
}else{
_4d.x=_4e.x=dim.width-_48.r;
_50=(_5d&&_5d=="axis")?90:270;
_4f.x=dim.width-_48.r+_51-(_50==270?0:_65);
switch(_56){
case "start":
_56="end";
break;
case "end":
_56="start";
break;
case "middle":
_55.x+=_62;
break;
}
}
}else{
_4d={x:_48.l};
_4e={x:dim.width-_48.r};
_4f={x:(dim.width-_48.r+_48.l)/2};
_51=_62*_63+(_66||0)*_64+_57+Math.max(_5e.length>0?_5e.length:0,_5f.length>0?_5f.length:0)+_65+_5c;
_52={x:_49?-1:1,y:0};
_55={x:0,y:0};
_53={x:0,y:1};
_54={x:0,y:_57};
switch(_4c){
case 0:
_56="middle";
_55.y=_62;
break;
case 90:
_56="start";
_55.x=-_62*0.4;
break;
case 180:
_56="middle";
break;
case 270:
_56="end";
_55.x=_62*0.4;
break;
default:
if(_4c<(90-_b)){
_56="start";
_55.y=_4b?_62:0;
}else{
if(_4c<(90+_b)){
_56="start";
_55.x=-_62*0.4;
}else{
if(_4c<180){
_56="start";
_55.y=_4b?0:-_62;
}else{
if(_4c<(270-_b)){
_56="end";
_55.y=_4b?0:-_62;
}else{
if(_4c<(270+_b)){
_56="end";
_55.y=_4b?_62*0.4:0;
}else{
_56="end";
_55.y=_4b?_62:0;
}
}
}
}
}
}
if(_4b){
_4d.y=_4e.y=_4a==="center"?dim.height/2:dim.height-_48.b;
_50=(_5d&&_5d=="axis")?180:0;
_4f.y=dim.height-_48.b+_51-(_50?_65:0);
}else{
_4d.y=_4e.y=_48.t;
_50=(_5d&&_5d=="away")?180:0;
_4f.y=_48.t-_51+(_50?0:_65);
_53.y=-1;
_54.y=-_54.y;
switch(_56){
case "start":
_56="end";
break;
case "end":
_56="start";
break;
case "middle":
_55.y-=_62;
break;
}
}
}
this.cleanGroup();
var s=this.group,c=this.scaler,t=this.ticks,f=_8.getTransformerFromModel(this.scaler),_67=(!o.title||!_50)&&!_4c&&this.opt.htmlLabels&&!_3("ie")&&!_3("opera")?"html":"gfx",dx=_53.x*_5e.length,dy=_53.y*_5e.length,_68=this._skipInterval;
s.createLine({x1:_4d.x,y1:_4d.y,x2:_4e.x,y2:_4e.y}).setStroke(_61);
if(o.title){
var _69=_9.createText[_67](this.chart,s,_4f.x,_4f.y,"middle",o.title,_59,_5b);
if(_67=="html"){
this.htmlElements.push(_69);
}else{
_69.setTransform(g.matrix.rotategAt(_50,_4f.x,_4f.y));
}
}
if(t==null){
this.dirty=false;
return this;
}
var rel=(t.major.length>0)?(t.major[0].value-this._majorStart)/c.major.tick:0;
var _6a=this.opt.majorLabels;
_2.forEach(t.major,function(_6b,i){
var _6c=f(_6b.value),_6d,x=(_49?_4e.x:_4d.x)+_52.x*_6c,y=_4d.y+_52.y*_6c;
i+=rel;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_5e);
if(_6b.label&&(!_68||(i-(1+_68))%(1+_68)==0)){
var _6e=o.maxLabelCharCount?this.getTextWithLimitCharCount(_6b.label,_58,o.maxLabelCharCount):{text:_6b.label,truncated:false};
_6e=o.maxLabelSize?this.getTextWithLimitLength(_6e.text,_58,o.maxLabelSize,_6e.truncated):_6e;
_6d=this.createText(_67,s,x+(_5e.length>0?dx:0)+_54.x+(_4c?0:_55.x),y+(_5e.length>0?dy:0)+_54.y+(_4c?0:_55.y),_56,_6e.text,_58,_5a);
if(_6e.truncated){
this.chart.formatTruncatedLabel(_6d,_6b.label,_67);
}
_6e.truncated&&this.labelTooltip(_6d,this.chart,_6b.label,_6e.text,_58,_67);
if(_67=="html"){
this.htmlElements.push(_6d);
}else{
if(_4c){
_6d.setTransform([{dx:_55.x,dy:_55.y},g.matrix.rotategAt(_4c,x+(_5e.length>0?dx:0)+_54.x,y+(_5e.length>0?dy:0)+_54.y)]);
}
}
}
},this);
dx=_53.x*_5f.length;
dy=_53.y*_5f.length;
_6a=this.opt.minorLabels&&c.minMinorStep<=c.minor.tick*c.bounds.scale;
_2.forEach(t.minor,function(_6f){
var _70=f(_6f.value),_71,x=(_49?_4e.x:_4d.x)+_52.x*_70,y=_4d.y+_52.y*_70;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_5f);
if(_6a&&_6f.label){
var _72=o.maxLabelCharCount?this.getTextWithLimitCharCount(_6f.label,_58,o.maxLabelCharCount):{text:_6f.label,truncated:false};
_72=o.maxLabelSize?this.getTextWithLimitLength(_72.text,_58,o.maxLabelSize,_72.truncated):_72;
_71=this.createText(_67,s,x+(_5f.length>0?dx:0)+_54.x+(_4c?0:_55.x),y+(_5f.length>0?dy:0)+_54.y+(_4c?0:_55.y),_56,_72.text,_58,_5a);
if(_72.truncated){
this.chart.formatTruncatedLabel(_71,_6f.label,_67);
}
_72.truncated&&this.labelTooltip(_71,this.chart,_6f.label,_72.text,_58,_67);
if(_67=="html"){
this.htmlElements.push(_71);
}else{
if(_4c){
_71.setTransform([{dx:_55.x,dy:_55.y},g.matrix.rotategAt(_4c,x+(_5f.length>0?dx:0)+_54.x,y+(_5f.length>0?dy:0)+_54.y)]);
}
}
}
},this);
dx=_53.x*_60.length;
dy=_53.y*_60.length;
_2.forEach(t.micro,function(_73){
var _74=f(_73.value),x=_4d.x+_52.x*_74,y=_4d.y+_52.y*_74;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_60);
},this);
this.dirty=false;
return this;
},labelTooltip:function(_75,_76,_77,_78,_79,_7a){
var _7b=["dijit/Tooltip"];
var _7c={type:"rect"},_7d=["above","below"],_7e=g._base._getTextBox(_78,{font:_79}).w||0,_7f=_79?g.normalizedLength(g.splitFontString(_79).size):0;
if(_7a=="html"){
_1.mixin(_7c,_6.position(_75.firstChild,true));
_7c.width=Math.ceil(_7e);
_7c.height=Math.ceil(_7f);
this._events.push({shape:dojo,handle:_5.connect(_75.firstChild,"onmouseover",this,function(e){
require(_7b,function(_80){
_80.show(_77,_7c,_7d);
});
})});
this._events.push({shape:dojo,handle:_5.connect(_75.firstChild,"onmouseout",this,function(e){
require(_7b,function(_81){
_81.hide(_7c);
});
})});
}else{
var shp=_75.getShape(),lt=_76.getCoords();
_7c=_1.mixin(_7c,{x:shp.x-_7e/2,y:shp.y});
_7c.x+=lt.x;
_7c.y+=lt.y;
_7c.x=Math.round(_7c.x);
_7c.y=Math.round(_7c.y);
_7c.width=Math.ceil(_7e);
_7c.height=Math.ceil(_7f);
this._events.push({shape:_75,handle:_75.connect("onmouseenter",this,function(e){
require(_7b,function(_82){
_82.show(_77,_7c,_7d);
});
})});
this._events.push({shape:_75,handle:_75.connect("onmouseleave",this,function(e){
require(_7b,function(_83){
_83.hide(_7c);
});
})});
}
},_isRtl:function(){
return false;
}});
return _3("dojo-bidi")?_4("dojox.charting.axis2d.Default",[_c,_a]):_c;
});
