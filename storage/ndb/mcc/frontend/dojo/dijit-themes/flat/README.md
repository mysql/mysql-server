### Flat Theme

A flat theme for Dojo Dijit.

![Image of Dojo flat theme](https://cloud.githubusercontent.com/assets/4641297/9564775/6cae1f44-4e65-11e5-8415-5c8b5b68875a.png)

**To Do:**

1. Fix any outlying dijits using opacity for disabled.
2. TitlePane and Accordion headers should have a separate mixin function from buttons.
3. Alternate color classes TitlePane headers.
4. Alternate color classes for AccordionContainer (active pane header).
5. Alternate color classes for Sliders; and remove transparency for disabled.

**Icons:**

All icons are Material Design icons by Google ([LICENSE](https://github.com/google/material-design-icons/blob/master/LICENSE)), and were generated using the [IcoMoon App](https://icomoon.io/app).

Icons, along with class names, `.dijitIcon*` aliases and hex codes, can be viewed in Flat Theme Test app.

This theme includes the `selection.json` file, which can be loaded into the IcoMoon App for editing. This icon font includes all the icons needed for icons used in dijits (close in Dialog, etc) and all aliased `.dijitIcon*` and `.dijitEditorIcon*` classes. The hex codes should be maintained as is.

The test app uses the `selection.json` to create the icons tests. The test app includes a globally exposed method `createIconClasses()`, which will create and download a text file containing the icon classes and dijit aliases, which can then be added to `flat-icons.sty` when changes are made to the icon set.

NOTE: This icon set is incomplete. Many of the `.dijitIcon*` and `.dijitEditorIcon*` icons do not have suitable icons in the Google icon set. These icons have a placeholder and have been assigned hex codes. Instead of using icons which do not quite fit the use, or from different icon sets, SVGs for these icons need to be created and added to the font. The SVGs need to be created on a 24 x 24 grid and generally be of the same design as the other icons.

**Issues:**

1. Toggle button doesn't maintain width when unchecked.
2. For input dijits with alternate color and required/validate, the border should also change via `.dijitTextBoxError` and equivalents. Alternate color styles are overriding.
3. Hover and selected calendar days with background 50% border radius looks a bit off on some days.
4. `.dijitIcon` class can cause issues when using certain icon fonts.

**Improvements:**

1. Consider typography, complimentary styling for native elements, helper classes, etc; and some components like Bootstrap.
2. Alternate colors for text input dijits, checkboxes, radio buttons, sliders, title pane, tooltips.
