The dgrid flat theme builds on the Stylus-based CSS refactor found in dgrid 1.0.
This portion of the theme requires the [dgrid](http://dgrid.io/) package.

To build your own flat dgrid theme, you may want to adjust the path to dgrid. By
default, we assume that dgrid is a sibling directory to themes. If this is not
the case within your application directory structure, simply specify a different
value for `$flat_path_to_dgrid` in your stylus file for your theme, before
importing themes/flat/dgrid/flat.styl.
