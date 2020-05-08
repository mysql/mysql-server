define([
	'intern!object',
	'intern/chai!assert',
	'../../on',
	'../../Evented',
	'dojo/_base/lang',
	'dojo/_base/array',
	'dojo/has',
	'dojo/has!host-browser?dojo/dom-construct',

	// Included to test on.selector
	'dojo/has!host-browser?../../query',
	'dojo/has!host-browser?dojo/domReady!'
], function (registerSuite, assert, on, Evented, lang, arrayUtil, has, domConstruct) {

	var handles = [];

	var originalOn = on;
	on = function () {
		var handle = originalOn.apply(null, arguments);
		handles.push(handle);
		return handle;
	};
	for (var key in originalOn) {
		on[key] = originalOn[key];
	}

	function cleanUpListeners() {
		while (handles.length > 0) {
			handles.pop().remove();
		}
	}

	function createCommonTests(args) {
		var target,
			testEventName = args.eventName;
		return {
			beforeEach: function () {
				target = args.createTarget();
			},
			afterEach: function () {
				// This would ideally be specified in a suite-wide afterEach,
				// but Safari throws exceptions if listener clean-up occurs after DOM nodes are destroyed
				cleanUpListeners();

				args.destroyTarget && args.destroyTarget(target);
			},

			'on and on.emit': function () {
				var listenerCallCount = 0,
					emittedEvent;

				on(target, testEventName, function (actualEvent) {
					listenerCallCount++;
					assert.strictEqual(actualEvent.value, emittedEvent.value);
				});

				emittedEvent = { value: 'foo' };
				on.emit(target, testEventName, emittedEvent);
				assert.strictEqual(listenerCallCount, 1);

				emittedEvent = { value: 'bar' };
				on.emit(target, testEventName, emittedEvent);
				assert.strictEqual(listenerCallCount, 2);
			},

			'.emit return value': function () {
				// NOTE: Uncancelable events are treated inconsistently.
				// The return value depends on whether the browser DOM offers
				// an `addEventListener` method and whether the event is a DOM or simple object event.
				// Therefore, the purpose of testing the return value for uncancelable events
				// is to codify current behavior and catch unintentional changes.
				var returnValue = on.emit(target, testEventName, { cancelable: false });
				if (has('dom-addeventlistener') && 'dispatchEvent' in target) {
					assert.ok(returnValue);
					assert.propertyVal(returnValue, 'cancelable', false);
				}
				else {
					assert.isFalse(returnValue);
				}

				returnValue = on.emit(target, testEventName, { cancelable: true });
				assert.ok(returnValue);
				assert.propertyVal(returnValue, 'cancelable', true);

				on(target, testEventName, function (event) {
					if ('preventDefault' in event) {
						event.preventDefault();
					}
					else {
						event.cancelable = false;
					}
				});
				assert.isFalse(on.emit(target, testEventName, { cancelable: true }));
			},

			'on - multiple event names': function () {
				var listenerCallCount = 0,
					emittedEventType,
					emittedEvent;

				on(target, 'test1, test2', function (actualEvent) {
					listenerCallCount++;
					if (emittedEventType in actualEvent) {
						assert.strictEqual(actualEvent.type, emittedEventType);
					}
					assert.strictEqual(actualEvent.value, emittedEvent.value);
				});

				emittedEventType = 'test1';
				emittedEvent = { value: 'foo' };
				on.emit(target, emittedEventType, emittedEvent);
				assert.strictEqual(listenerCallCount, 1);

				emittedEventType = 'test2';
				emittedEvent = { value: 'bar' };
				on.emit(target, emittedEventType, emittedEvent);
				assert.strictEqual(listenerCallCount, 2);
			},

			'on - multiple handlers': function () {
				var order = [];
				var customEvent = function (target, listener) {
					return on(target, 'custom', listener);
				};
				on(target, 'a, b', function (event) {
					order.push(1 + event.type);
				});
				on(target, [ 'a', customEvent ], function (event) {
					order.push(2 + event.type);
				});
				on.emit(target, 'a', { type: 'a' });
				on.emit(target, 'b', { type: 'b' });
				on.emit(target, 'custom', { type: 'custom' });
				assert.deepEqual(order, [ '1a', '2a', '1b', '2custom' ]);
			},

			'on - extension events': function () {
				var listenerCallCount = 0,
					emittedEvent,
					extensionEvent = function (target, listener) {
						return on(target, testEventName, listener);
					};

				on(target, extensionEvent, function (actualEvent) {
					listenerCallCount++;
					assert.strictEqual(actualEvent.value, emittedEvent.value);
				});

				emittedEvent = { value: 'foo' };
				on.emit(target, testEventName, emittedEvent);
				assert.strictEqual(listenerCallCount, 1);

				emittedEvent = { value: 'bar' };
				on.emit(target, testEventName, emittedEvent);
				assert.strictEqual(listenerCallCount, 2);
			},

			'.pausable': function () {
				var listenerCallCount = 0,
					handle = on.pausable(target, testEventName, function () {
						listenerCallCount++;
					});

				on.emit(target, testEventName, {});
				assert.strictEqual(listenerCallCount, 1);

				handle.pause();
				on.emit(target, testEventName, {});
				assert.strictEqual(listenerCallCount, 1);

				handle.resume();
				on.emit(target, testEventName, {});
				assert.strictEqual(listenerCallCount, 2);
			},

			'.once': function () {
				var listenerCallCount = 0;

				on.once(target, testEventName, function () {
					++listenerCallCount;
				});

				assert.strictEqual(listenerCallCount, 0);
				on.emit(target, testEventName, {});
				assert.strictEqual(listenerCallCount, 1);
				on.emit(target, testEventName, {});
				assert.strictEqual(listenerCallCount, 1);
			},

			'hitch no selector': function () {
				var div = document.body.appendChild(document.createElement("div"));
				var span = div.appendChild(document.createElement("span"));
				var button = span.appendChild(document.createElement("button"));
				var that = { fake: true };
				var result = {
					contextIsOk: false,
					selectorTargetIsOk: false
				}

				on(div, "click", lang.hitch(that, function (e) {
					result.contextIsOk = this === that;
					result.selectorTargetIsOk = e.selectorTarget === undefined; //selectorTarget is only available with event delegation
				}));

				on.emit(button, 'click', { cancelable: true, bubbles: true });

				assert.isTrue(result.contextIsOk, "contextIsOk");
				assert.isTrue(result.selectorTargetIsOk, "selectorTargetIsOk");
			},
			'hitch with selector': function () {
				var div = document.body.appendChild(document.createElement("div"));
				var span = div.appendChild(document.createElement("span"));
				var button = span.appendChild(document.createElement("button"));
				var that = { fake: true };
				var result = {
					contextIsOk: false,
					selectorTargetIsOk: false
				}

				on(div, "span:click", lang.hitch(that, function (e) {
					result.contextIsOk = this === that;
					result.selectorTargetIsOk = e.selectorTarget === span;
				}));

				on.emit(button, 'click', { cancelable: true, bubbles: true });

				assert.isTrue(result.contextIsOk, "contextIsOk");
				assert.isTrue(result.selectorTargetIsOk, "selectorTargetIsOk");
			},

			'listener call order': function () {
				var order = [],
					onMethodName = 'on' + testEventName;

				target[onMethodName] = function (event) {
					order.push(event.a);
				};
				var signal = on.pausable(target, testEventName, function () {
					order.push(1);
				});
				var signal2 = on(target, testEventName + ', foo', function (event) {
					order.push(event.a);
				});
				on.emit(target, testEventName, {
					a: 3
				});
				signal.pause();
				var signal3 = on(target, testEventName, function () {
					order.push(3);
				}, true);
				on.emit(target, testEventName, {
					a: 3
				});
				signal2.remove();
				signal.resume();
				on.emit(target, testEventName, {
					a: 6
				});
				signal3.remove();
				on(target, 'foo, ' + testEventName, function () {
					order.push(4);
				}, true);
				signal.remove();
				on.emit(target, testEventName, {
					a: 7
				});
				assert.deepEqual(order,  [ 3, 1, 3, 3, 3, 3, 6, 1, 3, 7, 4 ]);
			}
		};
	}

	var suite = {
		name: 'dojo/on',

		common: {
			'object events': createCommonTests({
				eventName: 'test',
				createTarget: function () {
					return new Evented();
				}
			})
		},

		'cannot target non-emitter': function () {
			var threwError = false;
			try {
				var nonEmitter = {};
				on(nonEmitter, 'test', function () {});
			}
			catch (err) {
				threwError = true;
			}
			assert.isTrue(threwError);
		}
	};

	if (has('host-browser')) {
		suite.common['DOM events'] = createCommonTests({
			eventName: 'click',
			createTarget: function () {
				return domConstruct.create('div', null, document.body);
			},
			destroyTarget: function (target) {
				domConstruct.destroy(target);
			}
		});

		// TODO: Add test to cover syntheticStopPropagation
		// TODO: Add tests to cover functionality of _fixEvent
		var containerDiv,
			childSpan;
		suite['DOM-specific'] = {

			'beforeEach': function () {
				containerDiv = domConstruct.create('div', null, document.body);
				childSpan = domConstruct.create('span', null, containerDiv);
			},
			'afterEach': function () {
				cleanUpListeners();

				domConstruct.destroy(containerDiv);
				containerDiv = childSpan = null;
			},

			'event.preventDefault': {
				'native event': function () {
					var defaultPrevented = false;

					on(childSpan, 'click', function (event) {
						event.preventDefault();
						defaultPrevented = event.defaultPrevented;
					});

					childSpan.click();
					assert.isTrue(defaultPrevented);
				},
				'synthetic event': function () {
					var secondListenerCalled = false,
						defaultPrevented = false;
					on(childSpan, 'click', function (event) {
						event.preventDefault();
					});
					on(containerDiv, 'click', function (event) {
						secondListenerCalled = true;
						defaultPrevented = event.defaultPrevented;
					});
					on.emit(childSpan, 'click', {bubbles: true, cancelable: true});
					assert.isTrue(secondListenerCalled, 'bubbled synthetic event on div');
					assert.isTrue(defaultPrevented, 'defaultPrevented set for synthetic event on div');
				}
			},

			'event bubbling': function () {
				var eventBubbled = false;

				on(containerDiv, 'click', function () {
					eventBubbled = true;
				});

				childSpan.click();
				assert.isTrue(eventBubbled, 'expected event to bubble');
			},

			'event.stopPropagation': function () {
				var eventBubbled;

				on(containerDiv, 'click', function () {
					eventBubbled = true;
				});
				on(childSpan, 'click', function (event) {
					event.stopPropagation();
				});

				// Testing with both Element#click and on.emit because they exercise different
				// code paths, most notably with the browsers that require synthetic dispatch and stopPropagation
				eventBubbled = false;
				childSpan.click();
				assert.isFalse(eventBubbled);

				eventBubbled = false;
				on.emit(childSpan, 'click', {});
				assert.isFalse(eventBubbled);
			},


			'event.stopImmediatePropagation': function () {
				on(childSpan, 'click', function (event) {
					event.stopImmediatePropagation();
				});

				var afterStop = false;
				on(childSpan, 'click', function () {
					afterStop = true;
				});

				childSpan.click();
				assert.isFalse(afterStop, 'expected no other listener to be called');
			},

			'emitting events from document and window': function () {
				// make sure 'document' and 'window' can also emit events
				var eventEmitted;
				var iframe = domConstruct.place('<iframe></iframe>', containerDiv);
				var globalObjects = [
					document, window, iframe.contentWindow, iframe.contentDocument || iframe.contentWindow.document
				];
				for (var i = 0, len = globalObjects.length; i < len; i++) {
					eventEmitted = false;
					on(globalObjects[i], 'custom-test-event', function () {
						eventEmitted = true;
					});
					on.emit(globalObjects[i], 'custom-test-event', {});
					assert.isTrue(eventEmitted);
				}
			},

			'event delegation': {
				'CSS selector': function () {
					var button = domConstruct.create('button', null, childSpan);

					var listenerCalled = false;
					on(containerDiv, 'button:click', function () {
						listenerCalled = true;
					});
					button.click();
					assert.isTrue(listenerCalled);
				},

				'listening on document': function () {
					var button = domConstruct.create('button', null, childSpan);

					var listenerCalled = false;
					on(document, 'button:click', function () {
						listenerCalled = true;
					});
					button.click();
					assert.isTrue(listenerCalled);
				},

				'CSS selector and text node target': function () {
					childSpan.className = 'textnode-parent';
					childSpan.innerHTML = 'text';

					var listenerCalled;
					on(containerDiv, '.textnode-parent:click', function () {
						listenerCalled = true;
					});

					on.emit(childSpan.firstChild, 'click', { bubbles: true, cancelable: true });
					assert.isTrue(listenerCalled);
				},

				'custom selector': function () {
					var button = domConstruct.create('button', null, childSpan);

					var listenerCalled = false;
					on(containerDiv, on.selector(function (node) {
						return node.tagName === 'BUTTON';
					}, 'click'), function () {
						listenerCalled = true;
					});

					button.click();
					assert.isTrue(listenerCalled);
				},

				'on.selector and extension events': {
					'basic extension events': function () {
						childSpan.setAttribute('foo', 2);
						var order = [];
						var customEvent = function (node, listener) {
							return on(node, 'custom', listener);
						};
						on(containerDiv, customEvent, function (event) {
							order.push(event.a);
						});
						on(containerDiv, on.selector('span', customEvent), function () {
							order.push(+this.getAttribute('foo'));
						});
						on.emit(containerDiv, 'custom', {
							a: 0
						});
						// should trigger selector
						on.emit(childSpan, 'custom', {
							a: 1,
							bubbles: true,
							cancelable: true
						});
						// shouldn't trigger selector
						on.emit(containerDiv, 'custom', {
							a: 3,
							bubbles: true,
							cancelable: true
						});
						assert.deepEqual(order, [0, 1, 2, 3]);
					},

					'extension events with bubbling forms': function () {
						var listenerCalled = false,
							bubbleListenerCalled = false;

						var customEvent = function (node, listener) {
							return on(node, 'custom', listener);
						};
						// simply test that an extension event's bubble method is applied if it exists
						customEvent.bubble = function (select) {
							return function (node, listener) {
								return customEvent(node, function (event) {
									bubbleListenerCalled = true;

									if (select(event.target)) {
										listener(event);
									}
								});
							};
						};

						on(containerDiv, on.selector('span', customEvent), function () {
							listenerCalled = true;
						});
						on.emit(childSpan, 'custom', { bubbles: true });
						assert.isTrue(listenerCalled);
						assert.isTrue(bubbleListenerCalled);
					}
				},

				'only call listener when matching': function () {
					containerDiv.innerHTML = '<input type="checkbox">';
					on(containerDiv, '.matchesNothing:click', function (event) {
						event.preventDefault();
					});
					containerDiv.firstChild.click();
					assert.isTrue(containerDiv.firstChild.checked);
				}
			},

			'event augmentation': function () {
				var button = domConstruct.create('button', null, containerDiv);
				on(button, 'click', function (event) {
					event.modified = true;
					event.test = 3;
				});
				var testValue;
				on(containerDiv, 'click', function (event) {
					testValue = event.test;
				});
				button.click();
				assert.strictEqual(testValue, 3);
			}
		};

		suite['.matches'] = (function () {
			var containerDiv2;

			return {
				beforeEach: function () {
					containerDiv = domConstruct.create('div', null, document.body);
					containerDiv2 = domConstruct.create('div', null, containerDiv);
					childSpan = domConstruct.create('span', null, containerDiv2);
				},

				afterEach: function () {
					cleanUpListeners();

					domConstruct.destroy(containerDiv);
					containerDiv = containerDiv2 = childSpan = null;
				},

				'inner-most child click': function () {
					on(containerDiv, 'click', function (event) {
						assert.ok(on.matches(event.target, 'span:click', this));
						assert.ok(on.matches(event.target, 'div:click', this));
						assert.ok(!on.matches(event.target, 'div:click', this, false));
						assert.ok(!on.matches(event.target, 'body:click', this));
					});
					childSpan.click();
				},

				'inner-most container click': function () {
					on(containerDiv, 'click', function (event) {
						assert.ok(!on.matches(event.target, 'span:click', this));
						assert.ok(on.matches(event.target, 'div:click', this));
					});
					containerDiv2.click();
				}
			};
		})();

		// TODO: Add tests to improve touch-related code coverage
		has('touch') && (suite['DOM-specific']['touch event normalization'] = function () {
			var div = document.body.appendChild(document.createElement('div'));

			var lastEvent;
			on(div, 'touchstart', function (event) {
				// Copying event properties to an object because certain versions of Firefox
				// threw insecure operation errors when saving an event to a closure-bound variable.
				lastEvent = lang.mixin({}, event);
			});
			// Since we can't simulate invoking TouchEvents (with current browsers initTouchEvent isn't available)
			// we will make TouchEvent be an Event temporarily so the `on` implementation
			// thinks that it is one for this test.
			var originalTouchEvent = window.TouchEvent;
			window.TouchEvent = Event;
			on.emit(div, 'touchstart', { changedTouches: [{ pageX: 100 }] });
			window.TouchEvent = originalTouchEvent;

			assert.property(lastEvent, 'rotation');
			assert.property(lastEvent, 'pageX');
		});

		has('touch') && (suite['DOM-specific']['touch event normalization doesn\'t happen to non-TouchEvent'] = function () {
			var div = document.body.appendChild(document.createElement('div'));

			var lastEvent;
			on(div, 'touchstart', function (event) {
				// Copying event properties to an object because certain versions of Firefox
				// threw insecure operation errors when saving an event to a closure-bound variable.
				lastEvent = lang.mixin({}, event);
			});
			on.emit(div, 'touchstart', { changedTouches: [{ pageX: 100 }] });

			assert.isFalse(lastEvent.pageX === 100);
		});
	}

	registerSuite(suite);
});
