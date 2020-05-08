define([
	"doh",
	"dojo/_base/lang",
	"dojox/mvc/at",
	"dojox/mvc/computed",
	"dojox/mvc/getStateful"
], function(doh, lang, at, computed, getStateful){
	var handles = [];
	function cleanHandles(){
		for(var handle = null; (handle = handles.shift());){
			if(typeof handle.close === "function"){
				handle.close();
			}else if(typeof handle.remove === "function"){
				handle.remove();
			}else if(typeof handle.destroyRecursive === "function"){
				handle.destroyRecursive();
			}else if(typeof handle.destroy === "function"){
				handle.destroy();
			}else{
				throw new Error("Handle cannot be cleaned up.");
			}
		}
	}

	doh.register("dojox.mvc.tests.doh.computed", [
		function checkParameters(t){
			var throwCount = 0;
			try{
				computed(undefined, "foo");
			}catch(e){
				++throwCount;
			}
			try{
				computed(null, "foo");
			}catch(e){
				++throwCount;
			}
			try{
				computed({}, "*");
			}catch(e){
				++throwCount;
			}
			try{
				computed({}, "foo", function(){}, at({}, "*"));
			}catch(e){
				++throwCount;
			}
			t.is(4, throwCount);
		},
		{
			name: "Computed property - Observable",
			tearDown: cleanHandles,
			runTest: function(t){
				var count = 0,
					stateful = getStateful({
						first: "John",
						last: "Doe"
					});

				handles.push(computed(stateful, "name", function(first, last){
					return first + " " + last;
				}, at(stateful, "first"), at(stateful, "last")));

				handles.push(computed(stateful, "nameLength", function(name){
					return name.length;
				}, at(stateful, "name")));

				handles.push(stateful.watch("name", function(name, old, current){
					t.is("Ben Doe", current);
					t.is("John Doe", old);
					count++;
				}));

				handles.push(stateful.watch("nameLength", function(name, old, current){
					t.is(7, current);
					t.is(8, old);
					count++;
				}));

				t.is("John Doe", stateful.get("name"));
				t.is(8, stateful.get("nameLength"));

				stateful.set("first", "Ben");
				t.is(2, count);
			}
		},
		{
			name: "Computed property - Non-observable",
			tearDown: cleanHandles,
			runTest: function(t){
				var o = {
					first: "John",
					last: "Doe"
				};

				handles.push(computed(o, "name", function(first, last){
					return first + " " + last;
				}, at(o, "first"), at(o, "last")));

				handles.push(computed(o, "nameLength", function(name){
					return name.length;
				}, at(o, "name")));

				t.is("John Doe", o.name);
				t.is(8, o.nameLength);
			}
		},
		{
			name: "Computed array - Observable",
			tearDown: cleanHandles,
			runTest: function(t){
				var count = 0,
					stateful = getStateful({
						items: [
							{name: "Anne Ackerman"},
							{name: "Ben Beckham"},
							{name: "Chad Chapman"},
							{name: "Irene Ira"}
						],
						countShortLessThanFour: true
					}),
					callbacks = [
						function(length, oldLength){
							t.is(57, length);
							t.is(45, oldLength);
							stateful.items[4].set("name", "John Doe");
						},
						function(length, oldLength){
							t.is(53, length);
							t.is(57, oldLength);
							stateful.set("countShortLessThanFour", false);
						},
						function(length, oldLength){
							t.is(42, length);
							t.is(53, oldLength);
						}
					];

				handles.push(computed(stateful, "totalNameLength", function(a, countShortLessThanFour){
					var total = 0;
					for(var i = 0; i < a.length; i++){
						var first = a[i].split(" ")[0];
						total += (countShortLessThanFour || first.length >= 4 ? a[i].length : 0);
					}
					return total;
				}, lang.mixin(at(stateful.items, "name"), {each: true}), at(stateful, "countShortLessThanFour")));

				handles.push(stateful.watch("totalNameLength", function(name, old, current){
					callbacks[count++](current, old);
				}));

				t.is(45, stateful.get("totalNameLength"));

				stateful.items.push(getStateful({name: "John Jacklin"}));
				t.is(3, count);
			}
		},
		{
			name: "Computed array - Non-observable",
			tearDown: cleanHandles,
			runTest: function(t){
				var o = {
					items: [
						{name: "Anne Ackerman"},
						{name: "Ben Beckham"},
						{name: "Chad Chapman"},
						{name: "Irene Ira"}
					],
					countShortLessThanFour: true
				};

				handles.push(computed(o, "totalNameLength", function (a, countShortLessThanFour) {
					var total = 0;
					for(var i = 0; i < a.length; i++){
						var first = a[i].split(" ")[0];
						total += (countShortLessThanFour || first.length >= 4 ? a[i].length : 0);
					}
					return total;
				}, lang.mixin(at(o.items, "name"), {each: true}), at(o, "countShortLessThanFour")));

				t.is(45, o.totalNameLength);
			}
		},
		{
			name: "Computed property in array",
			tearDown: cleanHandles,
			runTest: function(t){
				var called,
					statefulArray = getStateful(["foo"]);

				handles.push(computed(statefulArray, 1, function(foo){
					return "*" + foo + "*";
				}, at(statefulArray, 0)));

				handles.push(statefulArray.watch(1, function(name, old, current){
					t.is("*bar*", current);
					t.is("*foo*", old);
					called = true;
				}));

				t.is("*foo*", statefulArray[1]);

				statefulArray.set(0, "bar");
				t.t(called);
			}
		},
		{
			name: "Error in computed property callback",
			tearDown: cleanHandles,
			runTest: function(t){
				var count = 0,
					stateful = getStateful({
						first: "John",
						last: "Doe"
					});

				handles.push(computed(stateful, "name", function(first, last){
					if (first === "John") {
						throw undefined;
					}
					return first + " " + last;
				}, at(stateful, "first"), at(stateful, "last")));

				handles.push(computed(stateful, "nameLength", function(name){
					return name.length;
				}, at(stateful, "name")));

				handles.push(stateful.watch("name", function(name, old, current){
					t.is("Ben Doe", current);
					t.is("undefined", typeof old);
					count++;
				}));

				handles.push(stateful.watch("nameLength", function(name, old, current){
					t.is(7, current);
					t.is("undefined", typeof old);
					count++;
				}));

				t.is("undefined", typeof stateful.get("name"));
				t.is("undefined", typeof stateful.get("nameLength"));

				stateful.set("first", "Ben");
				t.is(2, count);
			}
		},
		{
			name: "Checking for same value",
			tearDown: cleanHandles,
			timeout: 1000,
			runTest: function(t){
				var dfd = new doh.Deferred(),
					shouldChange = true,
					count = 0,
					stateful = getStateful({
						foo: NaN
					});
				handles.push(computed(stateful, "computed", dfd.getTestErrback(function(foo){
					if (!shouldChange) {
						throw new Error("Change is detected even though there is no actual change.");
					}
					++count;
					return foo;
				}), at(stateful, "foo")));
				shouldChange = false;
				stateful.set("foo", NaN);
				shouldChange = true;
				stateful.set("foo", -0);
				stateful.set("foo", +0);
				t.is(3, count);
				dfd.callback(1);
			}
		},
		{
			name: "Cleaning up computed properties/arrays",
			tearDown: cleanHandles,
			runTest: function(){
				var stateful = getStateful({
					first: "John",
					last: "Doe",
					items: [
						{name: "Anne Ackerman"},
						{name: "Ben Beckham"},
						{name: "Chad Chapman"},
						{name: "Irene Ira"}
					]
				});

				var computeHandleName = computed(stateful, "name", function (first, last) {
					return first + " " + last;
				}, at(stateful, "first"), at(stateful, "last"));
				handles.push(computeHandleName);

				var computeHandleTotalNameLength = computed(stateful, "totalNameLength", function (names) {
					var total = 0;
					for(var i = 0; i < names.length; i++){
						total += names[i].length;
					}
					return total;
				}, lang.mixin(at(stateful.items, "name"), {each: true}));
				handles.push(computeHandleTotalNameLength);

				handles.push(stateful.watch("nameLength", function(){
					throw new Error("Watch callback shouldn't be called for removed computed property.");
				}));

				handles.push(stateful.watch("totalNameLength", function(){
					throw new Error("Watch callback shouldn't be called for removed computed array.");
				}));

				computeHandleName.remove();
				computeHandleTotalNameLength.remove();

				stateful.set("first", "Ben");
				stateful.items.push({name: "John Jacklin"});
			}
		}
	]);
});
