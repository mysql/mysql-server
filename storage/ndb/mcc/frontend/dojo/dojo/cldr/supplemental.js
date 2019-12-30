/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/cldr/supplemental",["../_base/lang","../i18n"],function(_1,_2){
var _3={};
_1.setObject("dojo.cldr.supplemental",_3);
_3.getFirstDayOfWeek=function(_4){
var _5={bd:5,mv:5,ae:6,af:6,bh:6,dj:6,dz:6,eg:6,iq:6,ir:6,jo:6,kw:6,ly:6,ma:6,om:6,qa:6,sa:6,sd:6,sy:6,ye:6,ag:0,ar:0,as:0,au:0,br:0,bs:0,bt:0,bw:0,by:0,bz:0,ca:0,cn:0,co:0,dm:0,"do":0,et:0,gt:0,gu:0,hk:0,hn:0,id:0,ie:0,il:0,"in":0,jm:0,jp:0,ke:0,kh:0,kr:0,la:0,mh:0,mm:0,mo:0,mt:0,mx:0,mz:0,ni:0,np:0,nz:0,pa:0,pe:0,ph:0,pk:0,pr:0,py:0,sg:0,sv:0,th:0,tn:0,tt:0,tw:0,um:0,us:0,ve:0,vi:0,ws:0,za:0,zw:0};
var _6=_3._region(_4);
var _7=_5[_6];
return (_7===undefined)?1:_7;
};
_3._region=function(_8){
_8=_2.normalizeLocale(_8);
var _9=_8.split("-");
var _a=_9[1];
if(!_a){
_a={aa:"et",ab:"ge",af:"za",ak:"gh",am:"et",ar:"eg",as:"in",av:"ru",ay:"bo",az:"az",ba:"ru",be:"by",bg:"bg",bi:"vu",bm:"ml",bn:"bd",bo:"cn",br:"fr",bs:"ba",ca:"es",ce:"ru",ch:"gu",co:"fr",cr:"ca",cs:"cz",cv:"ru",cy:"gb",da:"dk",de:"de",dv:"mv",dz:"bt",ee:"gh",el:"gr",en:"us",es:"es",et:"ee",eu:"es",fa:"ir",ff:"sn",fi:"fi",fj:"fj",fo:"fo",fr:"fr",fy:"nl",ga:"ie",gd:"gb",gl:"es",gn:"py",gu:"in",gv:"gb",ha:"ng",he:"il",hi:"in",ho:"pg",hr:"hr",ht:"ht",hu:"hu",hy:"am",ia:"fr",id:"id",ig:"ng",ii:"cn",ik:"us","in":"id",is:"is",it:"it",iu:"ca",iw:"il",ja:"jp",ji:"ua",jv:"id",jw:"id",ka:"ge",kg:"cd",ki:"ke",kj:"na",kk:"kz",kl:"gl",km:"kh",kn:"in",ko:"kr",ks:"in",ku:"tr",kv:"ru",kw:"gb",ky:"kg",la:"va",lb:"lu",lg:"ug",li:"nl",ln:"cd",lo:"la",lt:"lt",lu:"cd",lv:"lv",mg:"mg",mh:"mh",mi:"nz",mk:"mk",ml:"in",mn:"mn",mo:"ro",mr:"in",ms:"my",mt:"mt",my:"mm",na:"nr",nb:"no",nd:"zw",ne:"np",ng:"na",nl:"nl",nn:"no",no:"no",nr:"za",nv:"us",ny:"mw",oc:"fr",om:"et",or:"in",os:"ge",pa:"in",pl:"pl",ps:"af",pt:"br",qu:"pe",rm:"ch",rn:"bi",ro:"ro",ru:"ru",rw:"rw",sa:"in",sd:"in",se:"no",sg:"cf",si:"lk",sk:"sk",sl:"si",sm:"ws",sn:"zw",so:"so",sq:"al",sr:"rs",ss:"za",st:"za",su:"id",sv:"se",sw:"tz",ta:"in",te:"in",tg:"tj",th:"th",ti:"et",tk:"tm",tl:"ph",tn:"za",to:"to",tr:"tr",ts:"za",tt:"ru",ty:"pf",ug:"cn",uk:"ua",ur:"pk",uz:"uz",ve:"za",vi:"vn",wa:"be",wo:"sn",xh:"za",yi:"il",yo:"ng",za:"cn",zh:"cn",zu:"za",ace:"id",ady:"ru",agq:"cm",alt:"ru",amo:"ng",asa:"tz",ast:"es",awa:"in",bal:"pk",ban:"id",bas:"cm",bax:"cm",bbc:"id",bem:"zm",bez:"tz",bfq:"in",bft:"pk",bfy:"in",bhb:"in",bho:"in",bik:"ph",bin:"ng",bjj:"in",bku:"ph",bqv:"ci",bra:"in",brx:"in",bss:"cm",btv:"pk",bua:"ru",buc:"yt",bug:"id",bya:"id",byn:"er",cch:"ng",ccp:"in",ceb:"ph",cgg:"ug",chk:"fm",chm:"ru",chp:"ca",chr:"us",cja:"kh",cjm:"vn",ckb:"iq",crk:"ca",csb:"pl",dar:"ru",dav:"ke",den:"ca",dgr:"ca",dje:"ne",doi:"in",dsb:"de",dua:"cm",dyo:"sn",dyu:"bf",ebu:"ke",efi:"ng",ewo:"cm",fan:"gq",fil:"ph",fon:"bj",fur:"it",gaa:"gh",gag:"md",gbm:"in",gcr:"gf",gez:"et",gil:"ki",gon:"in",gor:"id",grt:"in",gsw:"ch",guz:"ke",gwi:"ca",haw:"us",hil:"ph",hne:"in",hnn:"ph",hoc:"in",hoj:"in",ibb:"ng",ilo:"ph",inh:"ru",jgo:"cm",jmc:"tz",kaa:"uz",kab:"dz",kaj:"ng",kam:"ke",kbd:"ru",kcg:"ng",kde:"tz",kdt:"th",kea:"cv",ken:"cm",kfo:"ci",kfr:"in",kha:"in",khb:"cn",khq:"ml",kht:"in",kkj:"cm",kln:"ke",kmb:"ao",koi:"ru",kok:"in",kos:"fm",kpe:"lr",krc:"ru",kri:"sl",krl:"ru",kru:"in",ksb:"tz",ksf:"cm",ksh:"de",kum:"ru",lag:"tz",lah:"pk",lbe:"ru",lcp:"cn",lep:"in",lez:"ru",lif:"np",lis:"cn",lki:"ir",lmn:"in",lol:"cd",lua:"cd",luo:"ke",luy:"ke",lwl:"th",mad:"id",mag:"in",mai:"in",mak:"id",man:"gn",mas:"ke",mdf:"ru",mdh:"ph",mdr:"id",men:"sl",mer:"ke",mfe:"mu",mgh:"mz",mgo:"cm",min:"id",mni:"in",mnk:"gm",mnw:"mm",mos:"bf",mua:"cm",mwr:"in",myv:"ru",nap:"it",naq:"na",nds:"de","new":"np",niu:"nu",nmg:"cm",nnh:"cm",nod:"th",nso:"za",nus:"sd",nym:"tz",nyn:"ug",pag:"ph",pam:"ph",pap:"bq",pau:"pw",pon:"fm",prd:"ir",raj:"in",rcf:"re",rej:"id",rjs:"np",rkt:"in",rof:"tz",rwk:"tz",saf:"gh",sah:"ru",saq:"ke",sas:"id",sat:"in",saz:"in",sbp:"tz",scn:"it",sco:"gb",sdh:"ir",seh:"mz",ses:"ml",shi:"ma",shn:"mm",sid:"et",sma:"se",smj:"se",smn:"fi",sms:"fi",snk:"ml",srn:"sr",srr:"sn",ssy:"er",suk:"tz",sus:"gn",swb:"yt",swc:"cd",syl:"bd",syr:"sy",tbw:"ph",tcy:"in",tdd:"cn",tem:"sl",teo:"ug",tet:"tl",tig:"er",tiv:"ng",tkl:"tk",tmh:"ne",tpi:"pg",trv:"tw",tsg:"ph",tts:"th",tum:"mw",tvl:"tv",twq:"ne",tyv:"ru",tzm:"ma",udm:"ru",uli:"fm",umb:"ao",unr:"in",unx:"in",vai:"lr",vun:"tz",wae:"ch",wal:"et",war:"ph",xog:"ug",xsr:"np",yao:"mz",yap:"fm",yav:"cm",zza:"tr"}[_9[0]];
}else{
if(_a.length==4){
_a=_9[2];
}
}
return _a;
};
_3.getWeekend=function(_b){
var _c={"in":0,af:4,dz:4,ir:4,om:4,sa:4,ye:4,ae:5,bh:5,eg:5,il:5,iq:5,jo:5,kw:5,ly:5,ma:5,qa:5,sd:5,sy:5,tn:5},_d={af:5,dz:5,ir:5,om:5,sa:5,ye:5,ae:6,bh:5,eg:6,il:6,iq:6,jo:6,kw:6,ly:6,ma:6,qa:6,sd:6,sy:6,tn:6},_e=_3._region(_b),_f=_c[_e],end=_d[_e];
if(_f===undefined){
_f=6;
}
if(end===undefined){
end=0;
}
return {start:_f,end:end};
};
return _3;
});
