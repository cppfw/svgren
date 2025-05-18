#include <svgren/render.hpp>

using namespace std::string_view_literals;

int main(int argc, const char** argv){
    auto dom = svgdom::load(R"qwertyuiop(
        <svg xmlns="http://www.w3.org/2000/svg" width="57.126" height="57.126">
            <path d="M28.563 0a28.563 28.563 0 100 57.126 28.563 28.563 0 100-57.126z" fill="#010101"/>
            <path d="M32.7 21.183c-.16-1.6-1.2-2.72-3.28-2.72-3.64 0-4.48 4.16-4.68 7.84l.08.08c.76-1.04 2.16-2.36 5.32-2.36 5.88 0 8.681 4.52 8.681 8.76 0 6.199-3.801 10.359-9.28 10.359-8.6 0-10.28-7.2-10.28-14.279 0-5.4.72-14.88 10.56-14.88 1.161 0 4.4.44 5.8 1.84 1.56 1.52 2.12 2.36 2.64 5.36h32.7zm-3.56 7.32c-2.12 0-4.28 1.32-4.28 4.88 0 3.08 1.76 5.28 4.44 5.28 2.04 0 3.92-1.561 3.92-5.4-.001-3.6-2.32-4.76-4.08-4.76z" fill="#fff"/>
        </svg>
    )qwertyuiop"sv);

    auto im = svgren::rasterize(*dom);

    std::cout << "im.dims() = " << im.dims() << std::endl;

    return 0;
}
