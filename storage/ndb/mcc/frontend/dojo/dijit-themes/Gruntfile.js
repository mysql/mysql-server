/* global module */
module.exports = function (grunt) {

  grunt.initConfig({

    // compile css
    stylus: {
      flat: {
        files: [{
          cwd: 'flat',
          dest: 'flat',
          expand: true,
          ext: '.css',
          src: ['**/*.styl', '!**/**mixins**.styl', '!**/**variables**.styl']
        }],
        options: {
          compress: false,
          linenos: false
        }
      }
    },

    connect: {
      flat: {
        options: {
          port: 3000,
          base: './',
          hostname: '*'
        }
      }
    },

    open: {
      flat: {
        path: 'http://localhost:3000/flat/tests/flat.html'
      }
    },

    watch: {
      flat: {
        files: ['flat/**/*.styl', '!flat/**/**variables.styl', '!flat/**/**mixins**.styl'],
        tasks: ['newer:stylus:flat']
      },
      'flat-vars': {
        files: ['flat/**/**variables**.styl', 'flat/**/**mixins**.styl'],
        tasks: ['stylus:flat']
      }
    },

    concurrent: {
      flat: {
        tasks: ['watch:flat', 'watch:flat-vars'],
        options: {
          logConcurrentOutput: true
        }
      }
    }
  });

  // load tasks
  grunt.loadNpmTasks('grunt-contrib-stylus');
  grunt.loadNpmTasks('grunt-contrib-connect');
  grunt.loadNpmTasks('grunt-open');
  grunt.loadNpmTasks('grunt-contrib-watch');
  grunt.loadNpmTasks('grunt-concurrent');
  grunt.loadNpmTasks('grunt-newer');

  // flat theme
  grunt.registerTask('flat', [
    'stylus:flat',
    'connect:flat',
    'open:flat',
    'concurrent:flat'
  ]);

};