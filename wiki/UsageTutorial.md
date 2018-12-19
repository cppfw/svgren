# Basic usage tutorial

Please note, that **svgren** uses C++'11 features, like *auto* etc.

First of all we need to include the **svgren** header file

``` cpp
#include <svgren/render.hpp>
#include <papki/FSFile.hpp> //we will need this to load the SVG file
```

Then we need to load the SVG file and create the document object model (DOM), let's load the file called *camera.svg*

``` cpp
auto dom = svgdom::load(papki::FSFile("camera.svg"));
```

Then we just render the SVG into a memory buffer

``` cpp
auto result = svgren::render(*dom);
//Returned 'result.pixels' is a std::vector<std::uint32_t> holding array of RGBA values.
```

If SVG document specifies any coordiantes or lengthes in physical units, like milimeters or centimeters or inches,
we have to supply the dots per inch (DPI) value of our physical display to the svgren::render() function. This is done using the `svgren::Parameters` structure

``` cpp
svgren::Parameters p;
p.bgra = true; // returned pixels format will be BGRA instead of RGBA
p.dpi = 240; // pixel density of target physical display is 240 dpi
p.widthRequest = 0; //0 means use width from SVG document
p.heightRequest = 0; //0 means use height from SVG document
auto result = svgren::render(*dom, p);
```

After that one can use the rendered image data to display it on any physical display or whatever.
