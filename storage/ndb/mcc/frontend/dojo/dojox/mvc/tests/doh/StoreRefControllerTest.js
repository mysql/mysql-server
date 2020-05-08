define([
	"doh",
	"dojo/_base/array",
	"dojo/_base/config",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/Deferred",
	"dojo/store/util/QueryResults",
	"dijit/_WidgetBase",
	"dojox/mvc/at",
	"dijit/_TemplatedMixin",
	"dijit/_WidgetsInTemplateMixin",
	"dijit/_Container",
	"dijit/form/TextBox",
	"dojox/mvc/getStateful",
    "dojox/mvc/StoreRefController",
    "dojox/mvc/EditStoreRefListController",	
    "dojo/store/JsonRest",
    "dojo/store/Memory",
    "dojo/store/Observable",
    "dojo/when",
	"dojox/mvc/WidgetList",
	"dojo/text!dojox/mvc/tests/test_WidgetList_WidgetListInTemplate.html",
	"dojo/text!dojox/mvc/tests/test_WidgetList_childTemplate.html",
	"dojo/text!dojox/mvc/tests/test_WidgetList_childBindings.json"
], function(doh, array, config, declare, lang, Deferred, QueryResults, _WidgetBase, at, _TemplatedMixin, _WidgetsInTemplateMixin, _Container,
	_TextBox, getStateful, StoreRefController, EditStoreRefListController, JsonRest, Memory, Observable, when, WidgetList, template, childTemplate, childBindings){7
    var data = {
        "identifier": "Serial",
        "items": [
            {
                "Serial"  : "A111",
                "First"   : "Anne",
                "Last"    : "Ackerman",
                "Email"   : "a.a@test.com"
            },
            {
                "Serial"  : "B111",
                "First"   : "Ben",
                "Last"    : "Beckham",
                "Email"   : "b.b@test.com"
            },
            {
                "Serial"  : "I111",
                "First"   : "Irene",
                "Last"    : "Ira",
                "Email"   : "i.i@test.com"
            },
            {
                "Serial"  : "J111",
                "First"   : "John",
                "Last"    : "Jacklin",
                "Email"   : "j.j@test.com"
            }
        ]
    };


	ctrl = new EditStoreRefListController({
		store : new Memory({
			data : data
		})
	});
	when(ctrl.getStore("A111"), function(value) {
		doh.register("dojox.mvc.tests.doh.StoreRefControllerTest", [
			function getStore(){
				doh.is(value.Serial, "A111", "Serial should be set");
				doh.is(value.First, "Anne", "First should be set");
				doh.is(value.Last, "Ackerman", "Last should be set");
				doh.is(value.Email, "a.a@test.com", "Email should be set");
			},
			function queryStore(){
				when(ctrl.queryStore(), function(results) {
					doh.is(results[0].Serial, "A111", "Serial should be set");
					doh.is(results[0].First, "Anne", "First should be set");
					doh.is(results[0].Last, "Ackerman", "Last should be set");
					doh.is(results[0].Email, "a.a@test.com", "Email should be set");
				});
			},
			function addStore(){
				var newId2 = "newObj222-" + Math.random();
				var newObj2 = {
					"Serial" : newId2,
					"First" : "newObj2",
					"Last" : "newObj2 Last",
					"Email" : "new.obj2@test.com"
				};
				when(ctrl.addStore(newObj2), function(results) {
					doh.is(results, newId2, "id should be returned");
					when(ctrl.getStore(newId2), function(value) {
						doh.is(value.Serial, newId2, "Serial should be set");
						doh.is(value.First, "newObj2", "First should be set");
						doh.is(value.Last, "newObj2 Last", "Last should be set");
						doh.is(value.Email, "new.obj2@test.com", "Email should be set");
					});
				});
			},
			function putStore(){
				var newId1 = "newObj111-" + Math.random();
				var newObj = {
					"Serial" : newId1,
					"First" : "newObj",
					"Last" : "newObj Last",
					"Email" : "new.obj@test.com"
				};
				when(ctrl.putStore(newObj), function(results) {
					doh.is(results, newId1, "id should be returned");
					when(ctrl.getStore(results), function(value) {
						doh.is(value.Serial, newId1, "Serial should be set");
						doh.is(value.First, "newObj", "First should be set");
						doh.is(value.Last, "newObj Last", "Last should be set");
						doh.is(value.Email, "new.obj@test.com", "Email should be set");
					});
				});
			},
			function removeStore(){
				when(ctrl.queryStore(), function(results) {
					var remObjId = results[1].Serial;
					when(ctrl.removeStore(remObjId), function(results) {
						doh.is(results, true, "should return true from removeStore");
					});
				});
			},
			function observableAsyncStore(){
				var AsyncMemoryStore = declare(Memory, {});
				for(var s in {put: 1, remove: 1, query: 1}){
					(function(s){
						AsyncMemoryStore.prototype[s] = function(){
							var args = arguments,
							 dfd = new Deferred(),
							 _self = this;
							setTimeout(function(){
								dfd.resolve(Memory.prototype[s].apply(_self, args));
							}, 500);
							return QueryResults(dfd.promise);
						};
					})(s);
				}
				var ctrl = new EditStoreRefListController({store: Observable(new AsyncMemoryStore({idProperty: "Serial", data: data}))}),
				 updates = [],
				 dfd = new doh.Deferred();
				ctrl.queryStore({Last: "Ackerman"}).observe(dfd.getTestErrback(function(object, previousIndex, newIndex){
					updates.push(lang.delegate(object, {
						previousIndex: previousIndex,
						newIndex: newIndex
					}));
					if(updates.length == 2){
						doh.is({
							Serial: "D111",
							First: "David",
							Last: "Ackerman",
							Email: "d.a@test.com",
							previousIndex: -1,
							newIndex: 1
						}, updates[0], "The observable callback should catch the addition");
						doh.is({
							Serial: "A111",
							First: "Anne",
							Last: "Ackerman",
							Email: "a.a@test.com",
							previousIndex: 0,
							newIndex: -1
						}, updates[1], "The observable callback should catch the removal");
						dfd.callback(1);
					}
				}));
				ctrl.addStore({
					Serial: "D111",
					First: "David",
					Last: "Ackerman",
					Email: "d.a@test.com"
				});
				ctrl.removeStore("A111");
				return dfd;
			},
			function observeJsonRest(){
				// Test we can observe results from a store that DOES defer results.
				var store = new JsonRest({
					target: require.toUrl("dojox/mvc/tests/_data/"),
					put: function(o){ var dfd = new Deferred(); setTimeout(function(){ dfd.resolve(o.id); }, 500); return dfd.promise; }, // Intead of making REST call, just return the ID asynchronously
					remove: function(){ var dfd = new Deferred(); setTimeout(function(){ dfd.resolve(true); }, 500); return dfd.promise; } // Intead of making REST call, just return true asynchronously
				});

				var ctrl = new StoreRefController({store : new Observable(store)}),
				 dfd = new doh.Deferred(),
				 updates = [];

				ctrl.queryStore("treeTestRoot").observe(dfd.getTestErrback(function(object, previousIndex, newIndex){
					updates.push(lang.delegate(object, {
						previousIndex: previousIndex,
						newIndex: newIndex
					}));
					if(updates.length == 2){
						doh.is({
							id: "node6",
							name: "node6",
							someProperty: "somePropertyA",
							previousIndex: -1,
							newIndex: 0
						}, updates[0], "The observable callback should catch the addition");
						doh.is({
							id: "node4",
							name: "node4",
							someProperty: "somePropertyA",
							previousIndex: 4,
							newIndex: -1
						}, updates[1], "The observable callback should catch the removal");
						dfd.callback(1);
					}
				}));
				ctrl.addStore({
					id: "node6",
					name: "node6",
					someProperty: "somePropertyA"
				});
				ctrl.removeStore("node4");
				return dfd;
			}
		]);
	}); 
});
