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
unsigned width = 0; //0 means use width from SVG document
unsigned height = 0; //0 means use height from SVG document
auto img = svgren::render(*dom, width, height); //uses 96 dpi by default
//At this point the 'width' and 'height' variables were filled with
//the actual width and height of the rendered image.
//Returned 'img' is a std::vector<std::uint32_t> holding array of RGBA values.
```

If SVG document specifies any coordiantes or lengthes in physical units, like milimeters or centimeters or inches,
we have to supply the dots per inch (DPI) value of our physical display to the svgren::render() function

``` cpp
unsigned width = 0; //0 means use width from SVG document
unsigned height = 0; //0 means use height from SVG document
auto img = svgren::render(*dom, width, height, 240); //240 dpi
```

After that one can use the rendered image data to display it on any physical display or whatever.
