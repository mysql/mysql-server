//>>built
define("dojox/data/JsonQueryRestStore",["dojo","dojox","dojox/data/JsonRestStore","dojox/data/util/JsonQuery","dojox/data/ClientFilter","dojox/json/query"],function(_1,_2){
_1.declare("dojox.data.JsonQueryRestStore",[_2.data.JsonRestStore,_2.data.util.JsonQuery],{matchesQuery:function(_3,_4){
return _3.__id&&(_3.__id.indexOf("#")==-1)&&this.inherited(arguments);
}});
return _2.data.JsonQueryRestStore;
});
