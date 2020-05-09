define([
	'intern!object',
	'intern/chai!assert',
	'../../Evented'
], function (registerSuite, assert, Evented) {
	registerSuite({
		name: 'dojo/Evented',

		'.on and .emit': function () {
			var evented = new Evented(),
				emittedEvent,
				listenerCallCount = 0;

			evented.on('test', function (actualEvent) {
				listenerCallCount++;
				assert.strictEqual(actualEvent.value, emittedEvent.value);
			});

			emittedEvent = { value: 'foo' };
			evented.emit('test', emittedEvent);
			assert.strictEqual(listenerCallCount, 1);

			emittedEvent = { value: 'bar' };
			evented.emit('test', emittedEvent);
			assert.strictEqual(listenerCallCount, 2);
		},

		'on<eventname> methods': function () {
			var evented = new Evented(),
				expectedEvent,
				actualEvent;

			assert.notProperty(evented, 'ontestevent');
			evented.on('testevent', function (event) {
				actualEvent = event;
			});
			assert.property(evented, 'ontestevent');

			expectedEvent = { value: 'test' };
			evented.ontestevent(expectedEvent);
			assert.deepEqual(actualEvent, expectedEvent);
		},

		'removing a listener': function () {
			var evented = new Evented(),
				listenerCalled = false;

			var handle = evented.on('test', function () {
				listenerCalled = true;
			});
			handle.remove();

			evented.emit('test', { value: 'foo' });
			assert.isFalse(listenerCalled);
		},

		'listener order': function () {
			var evented = new Evented();
			var order = [];

			evented.ontestevent = function () {
				order.push(1);
			};
			evented.on('testevent', function () {
				order.push(2);
			});
			evented.on('testevent', function () {
				order.push(3);
			});

			evented.emit('testevent', {});
			assert.deepEqual(order, [ 1, 2, 3 ]);
		}
	});
});
