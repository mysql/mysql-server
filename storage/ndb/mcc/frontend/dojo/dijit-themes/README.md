# themes
Dojo 1.x modern themes

While still being maintained, new development is primarily focused on modern Dojo.

Checkout the [Dojo framework website](https://dojo.io/) or if you want a more detailed technical status and overview, checkout the [`Dojo roadmap`](https://dojo.io/community/).

### Getting Started

1. Install globally Stylus and GruntJS.
 * `npm install -g stylus`
 * `npm install -g grunt-cli`
2. Run `grunt THEME_NAME` to compile css and open test application.

### Contributing New Themes

For general contributing guidelines see [Dojo's Contributing Guildlines](https://github.com/dojo/dojo/blob/master/CONTRIBUTING.md).

Adding a new theme:

1. Create a uniquely named directory, also being the name of the theme.
2. Add [Grunt](http://gruntjs.com/) tasks for the theme to `Gruntfile.js`.

Helpful information:

* [Dojo Documentation](http://dojotoolkit.org/documentation/)
* Kenneth Franqueiro's [dijit-claro-stylus](https://github.com/kfranqueiro/dijit-claro-stylus)
* [Stylus](https://learnboost.github.io/stylus/)
* [Grunt](http://gruntjs.com/)

## Themes

### Flat Theme

A flat theme for Dojo Dijit, Dojox and dgrid.

This theme is currently in **development**. Contributions welcome.

Run `grunt flat` to compile CSS and open Flat Theme's test application for development.

To use Flat Theme right away simply copy the `flat` directory and its contents to a location in your app, add/import `flat.css` and add `class="flat"` to the body tag.
