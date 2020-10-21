# Basic usage tutorial

Please note, that **svgren** uses C++'11 features, like *auto* etc.

First of all we need to include the **svgren** header file

``` cpp
#include <svgren/render.hpp>
#include <papki/fs_file.hpp> // we will need this to load the SVG file
```

Then we need to load the SVG file and create the document object model (DOM), let's load the file called *camera.svg*

``` cpp
auto dom = svgdom::load(papki::fs_file("camera.svg"));
```

Then we just render the SVG into a memory buffer

``` cpp
auto result = svgren::render(*dom);
// returned 'result.pixels' is a std::vector<uint32_t> holding array of RGBA values.
```

If SVG document specifies any coordiantes or lengthes in physical units, like milimeters or centimeters or inches,
we have to supply the dots per inch (DPI) value of our physical display to the svgren::render() function. This is done using the `svgren::parameters` structure

``` cpp
svgren::parameters p;
p.dpi = 240; // pixel density of target physical display is 240 dpi
p.dims_request.x() = 0; // 0 means use width from SVG document
p.dims_request.y() = 0; // 0 means use height from SVG document
auto result = svgren::render(*dom, p);
```

After that one can use the rendered image data to display it on any physical display or whatever.
