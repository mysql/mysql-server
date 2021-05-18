var path = require("path");

// the test-modules are not in the standard paths
//
// the first one should be local_modules/
// test_modules/ is just right next to it
module.paths.push(path.join(module.paths[0], "..", "test_modules"));

var m_direct = require("test-require-direct");
var m_dir_with_indexjs = require("test-require-dir-with-indexjs");
var m_dir_with_packagejson = require("test-require-dir-with-packagejson");
// 'require' the same module again. It shouldn't trigger a reload nor reset the counter
var m_no_reload = require("test-require-direct");

({
  stmts: function(stmt) {
    // all results share the same column-def
    var columns = {
      columns: [
        {
          name: "me",
          type: "STRING"
        }
      ]
    };

    if (stmt === "direct") {
      return {
        result: Object.assign({
          rows: [
            [ m_direct.me ]
          ]
        }, columns)
      }
    } else if (stmt === "dir-with-indexjs") {
      return {
        result: Object.assign({
          rows: [
            [ m_dir_with_indexjs.me ]
          ]
        }, columns)
      }
    } else if (stmt === "dir-with-packagejson") {
      return {
        result: Object.assign({
          rows: [
            [ m_dir_with_packagejson.me ]
          ]
        }, columns)
      }
    } else if (stmt === "no-reload-0") {
      return {
        result: Object.assign({
          rows: [
            [ m_direct.counter().toString() ]
          ]
        }, columns)
      }
    } else if (stmt === "no-reload-1") {
      return {
        result: Object.assign({
          rows: [
            [ m_no_reload.counter().toString() ]
          ]
        }, columns)
      }
    } else {
      return {
        error: {
          code: 1164,
          message: "don't know about " + stmt
        }
      }
    }
  }
})
