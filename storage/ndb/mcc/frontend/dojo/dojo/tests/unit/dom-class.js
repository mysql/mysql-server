define([
    'intern!object',
    'intern/chai!assert',
    '../../dom-class',
    'dojo/dom-construct',
    'dojo/sniff'

], function (registerSuite, assert, domClass, domConstruct) {

    var cssClass = 'test-css-class';
    var moreCssClass = 'test-adding-css-class';
    var multipleCss = 'existing-css-class';
    var value = 'the value';
    var node;
    var nodeIdIndex = 1;

    function generateId() {
        return 'nodeid_' + nodeIdIndex++;
    }

    registerSuite({
        name: 'dojo/dom-class',

        '.contains': {
            beforeEach: function () {
                node = domConstruct.toDom('<div class="' +
                    cssClass + '""></div>');
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            'node + class': function () {
                // when a node and class are passed, then true is returned

                assert.isTrue(domClass.contains(node, cssClass));
            },

            'string + class': function () {
                // when a node and class are passed, then true is returned

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                assert.isTrue(domClass.contains(nodeId, cssClass));
            },

            'node + unknown class': function () {
                // when a node and unknown class are passed, then false is returned

                assert.isFalse(domClass.contains(node, 'unknown-css-class'));
           },

            'string + unknown class': function () {
                // when a node id and unknown class are passed, then false is returned

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                assert.isFalse(domClass.contains(nodeId, 'unknown-css-class'));
           },

            'node w/ multiple classes + class': function () {
                // when a node and class are passed, then true is returned

                node.setAttribute('class', cssClass + ' ' + multipleCss);

                assert.isTrue(domClass.contains(node, cssClass));
                assert.isTrue(domClass.contains(node, multipleCss));
           },


            'string w/ multiple classes + class': function () {
                // when a node id and class are passed, then true is returned

                var nodeId = generateId();

                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + multipleCss);

                assert.isTrue(domClass.contains(node, cssClass));
                assert.isTrue(domClass.contains(node, multipleCss));
           },

            'node w/ multiple classes + unknown class': function () {
                // when a node and unknown class are passed, then false is returned

                node.setAttribute('class', cssClass + ' ' + multipleCss);

                assert.isFalse(domClass.contains(node, 'unknown-css-class'));
            },

            'string w/ multiple classes + unknown class': function () {
                // when a node id and unknown class are passed, then false is returned
                var nodeId = generateId();

                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + multipleCss);

                assert.isFalse(domClass.contains(nodeId, 'unknown-css-class'));
            },

            'document + class': function () {
                //contains on document should not throw error

                assert.isFalse(domClass.contains(document, 'unknown-css-class'));
            }
        },

        '.add': {
            beforeEach: function () {
                node = domConstruct.toDom('<div></div>');
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            'node + class': function () {
                // when a node and class are passed, then the class is added

                domClass.add(node, cssClass);

                assert.equal(node.getAttribute('class'), cssClass);
            },

            'string + class': function () {
                // when a node id and class are passed, then the class is added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.add(nodeId, cssClass);

                assert.equal(node.getAttribute('class'), cssClass);
            },

            'node + array': function () {
                // when a node and classes are passed, then the classes are added

                domClass.add(node, [cssClass, moreCssClass]);

                assert.equal(node.getAttribute('class'), [cssClass, moreCssClass].join(' '));
            },

            'string + array': function () {
                // when a node id and classes are passed, then the classes are added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.add(nodeId, [cssClass, moreCssClass]);

                assert.equal(node.getAttribute('class'), [cssClass, moreCssClass].join(' '));
            }
        },

        '.remove': {
            beforeEach: function () {
                node = domConstruct.toDom('<div class="' +
                    cssClass + '"></div>');
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            'node + undefined': function () {
                // when a node and undefined are passed, then all classes are removed

                domClass.remove(node, undefined);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + undefined': function () {
                // when a node id and undefined are passed, then all classes are removed

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.remove(nodeId, undefined);

                assert.equal(node.getAttribute('class'), '');
            },

            'node + class': function () {
                // when a node and class are passed, then the class is added

                domClass.remove(node, cssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + class': function () {
                // when a node id and class are passed, then the class is added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.remove(nodeId, cssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'node + classes': function () {
                // when a node and class are passed, then the class is added

                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.remove(node, cssClass + ' ' + moreCssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + classes': function () {
                // when a node id and class are passed, then the class is added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.remove(nodeId, cssClass + ' ' + moreCssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'node + array': function () {
                // when a node and classes are passed, then the classes are added

                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.remove(node, [cssClass, moreCssClass]);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + array': function () {
                // when a node id and classes are passed, then the classes are added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.remove(nodeId, [cssClass, moreCssClass]);

                assert.equal(node.getAttribute('class'), '');
            },

            'node w/ multiple + class': function () {
                // when a node and class are passed, then the class is added

                node.setAttribute('class', cssClass + ' ' + multipleCss);

                domClass.remove(node, cssClass);

                assert.equal(node.getAttribute('class'), multipleCss);
            },

            'string w/ multiple + class': function () {
                // when a node id and class are passed, then the class is added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + multipleCss);

                domClass.remove(nodeId, cssClass);

                assert.equal(node.getAttribute('class'), multipleCss);
            },

            'node w/ multiple + array': function () {
                // when a node and classes are passed, then the classes are added

                node.setAttribute('class', cssClass + ' ' + multipleCss + ' ' + moreCssClass);

                domClass.remove(node, [cssClass, moreCssClass]);

                assert.equal(node.getAttribute('class'), multipleCss);
            },

            'string w/ multiple + array': function () {
                // when a node id and classes are passed, then the classes are added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + multipleCss + ' ' + moreCssClass);

                domClass.remove(nodeId, [cssClass, moreCssClass]);

                assert.equal(node.getAttribute('class'), multipleCss);
            }
        },

        '.replace': {
            beforeEach: function () {
                node = domConstruct.toDom('<div class="' +
                    cssClass + '"></div>');
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            'node + add': function () {
                // when a node and add class is passed, the class is added.

                domClass.replace(node, moreCssClass);

                assert.equal(node.getAttribute('class'), moreCssClass);
            },

            'string + add': function () {
                // when a node id and add class is passed, the class is added.

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.replace(nodeId, moreCssClass);

                assert.equal(node.getAttribute('class'), moreCssClass);
            },

            'node + null add': function () {
                // when a node and remove class is passed, the class is removed.

                domClass.replace(node, null, cssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + null add': function () {
                // when a node id and remove class is passed, the class is removed.

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.replace(nodeId, null, cssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'node + null remove': function () {
                // when a node and add class is passed, the class is added

                domClass.replace(node, moreCssClass, null);

                assert.equal(node.getAttribute('class'), cssClass + ' ' + moreCssClass);
            },

            'string + null remove': function () {
                // when a node id and add class is passed, the class is added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.replace(nodeId, moreCssClass, null);

                assert.equal(node.getAttribute('class'), cssClass + ' ' + moreCssClass);
            },

            'node + class': function () {
                // when a node and add and remove class is passed, the classes are added and removed

                domClass.replace(node, moreCssClass, cssClass);

                assert.equal(node.getAttribute('class'), moreCssClass);
            },

            'string + class': function () {
                // when a node id and add and remove class is passed, the classes are added and removed

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.replace(nodeId, moreCssClass, cssClass);

                assert.equal(node.getAttribute('class'), moreCssClass);
            },

            'node + classes as string - removing': function () {
                // when a node and multiple classes are passed separated by a space, the classes are removed

                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.replace(node, null, moreCssClass + ' ' + cssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + classes as string - removing': function () {
                // when a node id and multiple classes are passed separated by a space, the classes are removed

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.replace(nodeId, null, moreCssClass + ' ' + cssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'node + classes as string - adding': function () {
                // when a node and multiple classes are passed separated by a space, the classes are added

                node.setAttribute('class', '');

                domClass.replace(node, moreCssClass + ' ' + cssClass);

                assert.equal(node.getAttribute('class'), moreCssClass + ' ' + cssClass);
            },

            'string + classes as string - adding': function () {
                // when a node id and multiple classes are passed separated by a space, the classes are added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', '');

                domClass.replace(nodeId, moreCssClass + ' ' + cssClass);

                assert.equal(node.getAttribute('class'), moreCssClass + ' ' + cssClass);
            },

            'node + classes as array - removing': function () {
                // when a node and multiple classes are passed as an array, the classes are removed

                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.replace(node, null, [moreCssClass, cssClass]);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + classes as array - removing': function () {
                // when a node id and multiple classes are passed as an array, the classes are removed

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.replace(nodeId, null, [moreCssClass, cssClass]);

                assert.equal(node.getAttribute('class'), '');
            },

            'node + classes as array - adding': function () {
                // when a node and multiple classes are passed as an array, the classes are added

                node.setAttribute('class', '');

                domClass.replace(node, [moreCssClass, cssClass]);

                assert.equal(node.getAttribute('class'), moreCssClass + ' ' + cssClass);
            },

            'string + classes as array - adding': function () {
                // when a node id and multiple classes are passed as an array, the classes are added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', '');

                domClass.replace(nodeId, [moreCssClass, cssClass]);

                assert.equal(node.getAttribute('class'), moreCssClass + ' ' + cssClass);
            },

            'node + classes as string - replacing': function () {
                // when a node and multiple classes are passed separated by a space, the classes are added

                domClass.replace(node, moreCssClass + ' ' + multipleCss, cssClass);

                assert.equal(node.getAttribute('class'), moreCssClass + ' ' + multipleCss);
            },

            'string + classes as string - replacing': function () {
                // when a node and multiple classes are passed separated by a space, the classes are added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.replace(nodeId, moreCssClass + ' ' + multipleCss, cssClass);

                assert.equal(node.getAttribute('class'), moreCssClass + ' ' + multipleCss);
            },

            'node + classes as array - replacing': function () {
                // when a node and multiple classes are passed as an array, the classes are removed

                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.replace(node, [multipleCss], [moreCssClass, cssClass]);

                assert.equal(node.getAttribute('class'), multipleCss);
            },

            'string + classes as array - replacing': function () {
                // when a node id and multiple classes are passed as an array, the classes are removed

                var nodeId = generateId();
                node.setAttribute('id', nodeId);
                node.setAttribute('class', cssClass + ' ' + moreCssClass);

                domClass.replace(nodeId, [multipleCss], [moreCssClass, cssClass]);

                assert.equal(node.getAttribute('class'), multipleCss);
            }
        },

        '.toggle': {
            beforeEach: function () {
                node = domConstruct.toDom('<div class="' +
                    cssClass + '"></div>');
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            'node + class': function () {
                // when you pass a node and class, the class is removed

                domClass.toggle(node, cssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + class': function () {
                // when you pass a node id and class, the class is removed

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.toggle(nodeId, cssClass);

                assert.equal(node.getAttribute('class'), '');
            },

            'node + class + true': function () {
                // when you pass a node and class and a true condition, the class is added

                domClass.toggle(node, cssClass, true);

                assert.equal(node.getAttribute('class'), cssClass);
            },

            'string + class + true': function () {
                // when you pass a node id and class and a true condition, the class is added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.toggle(nodeId, cssClass, true);

                assert.equal(node.getAttribute('class'), cssClass);
            },

            'node + class + false': function () {
                // when you pass a node and class and a false condition, the class is removed

                domClass.toggle(node, cssClass, false);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + class + false': function () {
                // when you pass a node id and class and a false condition, the class is removed

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.toggle(nodeId, cssClass, false);

                assert.equal(node.getAttribute('class'), '');
            },

            'node + new class': function () {
                // when you pass a node and new class, the class is added

                domClass.toggle(node, multipleCss);

                assert.equal(node.getAttribute('class'), cssClass + ' ' + multipleCss);
            },

            'string + new class': function () {
                // when you pass a node id and new class, the class is added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.toggle(nodeId, multipleCss);

                assert.equal(node.getAttribute('class'), cssClass + ' ' + multipleCss);
            },

            'node + array': function () {
                // when you pass a node and array, the classes are toggled

                domClass.toggle(node, [cssClass, multipleCss]);

                assert.equal(node.getAttribute('class'), multipleCss);
            },

            'string + array': function () {
                // when you pass a node id and array, the classes are toggled

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.toggle(nodeId, [cssClass, multipleCss]);

                assert.equal(node.getAttribute('class'), multipleCss);
            },

            'node + array + true': function () {
                // when you pass a node and array + true, the classes are added

                domClass.toggle(node, [cssClass, multipleCss], true);

                assert.equal(node.getAttribute('class'), cssClass + ' ' + multipleCss);
            },

            'string + array + true': function () {
                // when you pass a node id and array + true, the classes are added

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.toggle(nodeId, [cssClass, multipleCss], true);

                assert.equal( node.getAttribute('class'), cssClass + ' ' + multipleCss);
            },

            'node + classes': function () {
                // when you pass a node and multples classes, the classes are toggled

                domClass.toggle(node, cssClass + ' ' + multipleCss);

                assert.equal(node.getAttribute('class'), multipleCss);
            },

            'string + classes': function () {
                // when you pass a node id and multples classes, the classes are toggled

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.toggle(nodeId, cssClass + ' ' + multipleCss);

                assert.equal(node.getAttribute('class'), multipleCss);
            },

            'node + classes + false': function () {
                // when you pass a node and multples classes + false, the classes are removed

                domClass.toggle(node, cssClass + ' ' + multipleCss, false);

                assert.equal(node.getAttribute('class'), '');
            },

            'string + classes + false': function () {
                // when you pass a node id and multples classes + false, the classes are removed

                var nodeId = generateId();
                node.setAttribute('id', nodeId);

                domClass.toggle(nodeId, cssClass + ' ' + multipleCss, false);

                assert.equal(node.getAttribute('class'), '');
            }
        },

        'validation tests': {

            beforeEach: function () {
                node = domConstruct.toDom('<div></div>');
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            'testClassFunctions': function () {
                domClass.remove(node);
                domClass.add(node, 'a');
                assert.equal(node.className, 'a', 'class is a');

                domClass.remove(node, 'c');
                assert.equal(node.className, 'a', 'class is still a');
                assert.isTrue(domClass.contains(node, 'a'), 'class is a, test for a');
                assert.isFalse(domClass.contains(node, 'b'), 'class is a, test for b');

                domClass.add(node, 'b');
                assert.equal(node.className, 'a b', 'class is a b');
                assert.isTrue(domClass.contains(node, 'a'), 'class is a b, test for a');
                assert.isTrue(domClass.contains(node, 'b'), 'class is a b, test for b');

                domClass.remove(node, 'a');
                assert.equal(node.className, 'b', 'class is b');
                assert.isFalse(domClass.contains(node, 'a'), 'class is b, test for a');
                assert.isTrue(domClass.contains(node, 'b'), 'class is b, test for b');

                domClass.toggle(node, 'a');
                assert.equal(node.className, 'b a', 'class is b a');
                assert.isTrue(domClass.contains(node, 'a'), 'class is b a, test for a');
                assert.isTrue(domClass.contains(node, 'b'), 'class is b a, test for b');

                domClass.toggle(node, 'a');
                assert.equal(node.className, 'b', 'class is b (again)');
                assert.isFalse(domClass.contains(node, 'a'), 'class is b (again), test for a');
                assert.isTrue(domClass.contains(node, 'b'), 'class is b (again), test for b');

                domClass.toggle(node, 'b');
                assert.equal(node.className, '', 'class is blank');
                assert.isFalse(domClass.contains(node, 'a'), 'class is blank, test for a');
                assert.isFalse(domClass.contains(node, 'b'), 'class is blank, test for b');

                domClass.remove(node, 'c');
                assert.isTrue(!node.className, 'no class');

                var acuWorked = true;
                try{
                    domClass.add(node);
                }catch(e){
                    acuWorked = false;
                }
                assert.equal(acuWorked, true, 'addClass handles undefined class');

                domClass.add(node, 'a');
                domClass.replace(node, 'b', 'a');
                assert.isTrue(domClass.contains(node, 'b'), 'class is b after replacing a, test for b');
                assert.isFalse(domClass.contains(node, 'a'), 'class is b after replacing a, test for not a');

                domClass.replace(node, '', 'b');
                assert.isFalse(domClass.contains(node, 'b'), 'class b should be removed, with no class added');
                assert.equal(node.className, '', 'The className is empty');

                domClass.add(node, 'b a');
                domClass.replace(node, 'c', '');
                assert.equal(node.className,'b a c',
                    'The className is  is "b a c" after using an empty string in replaceClass');

                assert.isFalse(domClass.contains(document, 'ab'), 'hasClass on document should not throw error');
            },

            'testAddRemoveClassMultiple': function () {

                domClass.remove(node);
                domClass.add(node, 'a');
                assert.equal(node.className, 'a', 'class is a');

                domClass.add(node, 'a b');
                assert.equal(node.className, 'a b', 'class is a b');

                domClass.add(node, 'b a');
                assert.equal(node.className, 'a b', 'class is still a b');

                domClass.add(node, ['a', 'c']);
                assert.equal(node.className, 'a b c', 'class is a b c');

                domClass.remove(node, 'c a');
                assert.equal(node.className, 'b', 'class is b');

                domClass.remove(node);
                assert.equal(node.className, '', 'empty class');

                domClass.add(node, '  c   b   a ');
                assert.equal(node.className, 'c b a', 'class is c b a');

                domClass.remove(node, ' c b ');
                assert.equal(node.className, 'a', 'class is a');

                domClass.remove(node, ['a', 'c']);
                assert.equal(node.className, '', 'empty class');

                domClass.add(node, 'a b');
                domClass.replace(node, 'c', 'a b');
                assert.equal(node.className, 'c', 'class is c');

                domClass.replace(node, '');
                assert.equal(node.className, '', 'empty class');
            }
        }
    });
});