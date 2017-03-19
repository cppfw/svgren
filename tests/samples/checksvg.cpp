#include <iostream>
#include <vector>
#include <cstring>

#include <png.h>

#include <utki/Exc.hpp>
#include <kolme/Vector2.hpp>
#include <papki/FSFile.hpp>

#include "../../src/svgren/render.hpp"

class Image final{
public:
	/**
	 * @brief Image color depth.
	 */
	enum class ColorDepth_e{
		UNKNOWN = 0,
		GREY  = 1, //1 channel. Only Grey channel
		GREYA = 2, //2 channels. Grey with Alpha channel
		RGB   = 3, //3 channels. Red Green Blue channels
		RGBA  = 4  //4 channels. RGBA format (4 channels)
	};

	/**
	 * @brief Basic image exception.
	 */
	class Exc : public utki::Exc{
	public:
		Exc(const std::string& msg = std::string()) :
				utki::Exc(msg.c_str())
		{}
	};
	
	class IllegalArgumentExc : public Exc{
	public:
		IllegalArgumentExc(const std::string& msg = std::string()) :
				Exc(msg)
		{}
	};
	
private:
	ColorDepth_e colorDepth_v;
	kolme::Vec2ui dim_v = kolme::Vec2ui(0);
	std::vector<std::uint8_t> buf_v;//image pixels data

public:
	/**
	 * @brief Default constructor.
	 * Creates uninitialized Image object.
	 */
	Image() :
			colorDepth_v(ColorDepth_e::UNKNOWN)
	{}

	Image(const Image& im) = default;

	/**
	 * @brief Get image dimensions.
	 * @return Image dimensions.
	 */
	const kolme::Vec2ui& dim()const noexcept{
		return this->dim_v;
	}

	/**
	 * @brief Get color depth.
	 * @return Bits per pixel.
	 */
	unsigned bitsPerPixel()const{
		return this->numChannels() * 8;
	}

	/**
	 * @brief Get color depth.
	 * @return Number of color channels.
	 */
	unsigned numChannels()const{
		return unsigned(this->colorDepth_v);
	}

	/**
	 * @brief Get color depth.
	 * @return Color depth type.
	 */
	ColorDepth_e colorDepth()const{
		return this->colorDepth_v;
	}

	/**
	 * @brief Get pixel data.
	 * @return Pixel data of the image.
	 */
	utki::Buf<std::uint8_t> buf(){
		return utki::wrapBuf(this->buf_v);
	}

	/**
	 * @brief Get pixel data.
	 * @return Pixel data of the image.
	 */
	const utki::Buf<std::uint8_t> buf()const{
		return utki::wrapBuf(this->buf_v);
	}

public:
	/**
	 * @brief Initialize this image object with given parameters.
	 * Pixel data remains uninitialized.
	 * @param dimensions - image dimensions.
	 * @param colorDepth - color depth.
	 */
	void init(kolme::Vec2ui dimensions, ColorDepth_e colorDepth){
		this->dim_v = dimensions;
		this->colorDepth_v = colorDepth;
		this->buf_v.resize(this->dim().x * this->dim().y * this->numChannels());
	}
	

	/**
	 * @brief Flip image vertically.
	 */
	void flipVertical();


	/**
	 * @brief Get reference to specific channel for given pixel.
	 * @param x - X pixel location.
	 * @param y - Y pixel location.
	 * @param chan - channel index to get reference to.
	 * @return Reference to uint8_t representing a single color channel of given pixel.
	 */
	const std::uint8_t& pixChan(unsigned x, unsigned y, unsigned chan)const{
		return this->buf_v[ASSCOND((y * this->dim().x + x) * this->numChannels() + chan, < this->buf_v.size())];
	}

	/**
	 * @brief Get reference to specific channel for given pixel.
	 * @param x - X pixel location.
	 * @param y - Y pixel location.
	 * @param chan - channel number to get reference to.
	 * @return Reference to uint8_t representing a single color channel of given pixel.
	 */
	std::uint8_t& pixChan(unsigned x, unsigned y, unsigned chan){
		return this->buf_v[ASSCOND((y * this->dim().x + x) * this->numChannels() + chan, < this->buf_v.size())];
	}

private:
	static void PNG_CustomReadFunction(png_structp pngPtr, png_bytep data, png_size_t length){
		papki::File* fi = reinterpret_cast<papki::File*>(png_get_io_ptr(pngPtr));
		ASSERT_ALWAYS(fi)
	//	TRACE(<< "PNG_CustomReadFunction: fi = " << fi << " pngPtr = " << pngPtr << " data = " << std::hex << data << " length = " << length << std::endl)
		try{
			utki::Buf<png_byte> bufWrapper(data, size_t(length));
			fi->read(bufWrapper);
	//		TRACE(<< "PNG_CustomReadFunction: fi->Read() finished" << std::endl)
		}catch(...){
			//do not let any exception get out of this function
	//		TRACE(<< "PNG_CustomReadFunction: fi->Read() failed" << std::endl)
		}
	}
public:
	
	/**
	 * @brief Load image from PNG file.
	 * @param f - PNG file.
	 */
	void loadPNG(const papki::File& fi){
		ASSERT_ALWAYS(!fi.isOpened())

		ASSERT_ALWAYS(this->buf_v.size() == 0)

		papki::File::Guard fileGuard(fi);//this will guarantee that the file will be closed upon exit
	//	TRACE(<< "Image::LoadPNG(): file opened" << std::endl)

	#define PNGSIGSIZE 8 //The size of PNG signature (max 8 bytes)
		std::array<png_byte, PNGSIGSIZE> sig;
		memset(&*sig.begin(), 0, sig.size() * sizeof(sig[0]));

		{
	#ifdef DEBUG
			auto ret = //TODO: we should not rely on that it will always read the requested number of bytes
	#endif
			fi.read(utki::wrapBuf(sig));
			ASSERT_ALWAYS(ret == sig.size() * sizeof(sig[0]))
		}

		if(png_sig_cmp(&*sig.begin(), 0, sig.size() * sizeof(sig[0])) != 0){//if it is not a PNG-file
			throw Image::Exc("Image::LoadPNG(): not a PNG file");
		}

		//Great!!! We have a PNG-file!
	//	TRACE(<< "Image::LoadPNG(): file is a PNG" << std::endl)

		//Create internal PNG-structure to work with PNG file
		//(no warning and error callbacks)
		png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);

		png_infop infoPtr = png_create_info_struct(pngPtr);//Create structure with file info

		png_set_sig_bytes(pngPtr, PNGSIGSIZE);//We've already read PNGSIGSIZE bytes

		//Set custom "ReadFromFile" function
		png_set_read_fn(pngPtr, const_cast<papki::File*>(&fi), PNG_CustomReadFunction);

		png_read_info(pngPtr, infoPtr);//Read in all information about file

		//Get information from infoPtr
		png_uint_32 width = 0;
		png_uint_32 height = 0;
		int bitDepth = 0;
		int colorType = 0;
		png_get_IHDR(pngPtr, infoPtr, &width, &height, &bitDepth, &colorType, 0, 0, 0);

		//Strip 16bit png  to 8bit
		if(bitDepth == 16){
			png_set_strip_16(pngPtr);
		}
		//Convert paletted PNG to RGB image
		if(colorType == PNG_COLOR_TYPE_PALETTE){
			png_set_palette_to_rgb(pngPtr);
		}
		//Convert grayscale PNG to 8bit greyscale PNG
		if(colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8){
			png_set_expand_gray_1_2_4_to_8(pngPtr);
		}
		//if(png_get_valid(pngPtr, infoPtr,PNG_INFO_tRNS)) png_set_tRNS_to_alpha(pngPtr);

		//set gamma information
		double gamma = 0.0f;

		//if there's gamma info in the file, set it to 2.2
		if(png_get_gAMA(pngPtr, infoPtr, &gamma)){
			png_set_gamma(pngPtr, 2.2, gamma);
		}else{
			png_set_gamma(pngPtr, 2.2, 0.45455);//set to 0.45455 otherwise (good guess for GIF images on PCs)
		}

		//update info after all transformations
		png_read_update_info(pngPtr, infoPtr);
		//get all dimensions and color info again
		png_get_IHDR(pngPtr, infoPtr, &width, &height, &bitDepth, &colorType, 0, 0, 0);
		ASSERT_ALWAYS(bitDepth == 8)

		//Set image type
		Image::ColorDepth_e imageType;
		switch(colorType){
			case PNG_COLOR_TYPE_GRAY:
				imageType = Image::ColorDepth_e::GREY;
				break;
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				imageType = Image::ColorDepth_e::GREYA;
				break;
			case PNG_COLOR_TYPE_RGB:
				imageType = Image::ColorDepth_e::RGB;
				break;
			case PNG_COLOR_TYPE_RGB_ALPHA:
				imageType = Image::ColorDepth_e::RGBA;
				break;
			default:
				throw Image::Exc("Image::LoadPNG(): unknown colorType");
				break;
		}
		//Great! Number of channels and bits per pixel are initialized now!

		//set image dimensions and set buffer size
		this->init(kolme::Vec2ui(width, height), imageType);//Set buf array size (allocate memory)
		//Great! height and width are initialized and buffer memory allocated

	//	TRACE(<< "Image::LoadPNG(): memory for image allocated" << std::endl)

		//Read image data
		png_size_t bytesPerRow = png_get_rowbytes(pngPtr, infoPtr);//get bytes per row

		//check that our expectations are correct
		if(bytesPerRow != this->dim().x * this->numChannels()){
			throw Image::Exc("Image::LoadPNG(): number of bytes per row does not match expected value");
		}

		ASSERT_ALWAYS((bytesPerRow * height) == this->buf_v.size())

	//	TRACE(<< "Image::LoadPNG(): going to read in the data" << std::endl)
		{
			ASSERT_ALWAYS(this->dim().y && this->buf_v.size())
			std::vector<png_bytep> rows(this->dim().y);
			//initialize row pointers
	//		M_IMAGE_PRINT(<< "Image::LoadPNG(): this->buf.Buf() = " << std::hex << this->buf.Buf() << std::endl)
			for(unsigned i = 0; i < this->dim().y; ++i){
				rows[i] = &*this->buf_v.begin() + i * bytesPerRow;
	//			M_IMAGE_PRINT(<< "Image::LoadPNG(): rows[i] = " << std::hex << rows[i] << std::endl)
			}
	//		TRACE(<< "Image::LoadPNG(): row pointers are set" << std::endl)
			//Read in image data!
			png_read_image(pngPtr, &*rows.begin());
	//		TRACE(<< "Image::LoadPNG(): image data read" << std::endl)
		}

		png_destroy_read_struct(&pngPtr,0,0);//free libpng memory
	}
};




int main(int argc, char** argv) {
	if(argc != 3){
		std::cout << "Error: 2 arguments expected: <svg-file> <png-file>" << std::endl;
		return 1;
	}
	
	Image png;
	png.loadPNG(papki::FSFile(argv[2]));
	
	ASSERT_ALWAYS(png.buf().size() != 0)
	
	auto dom = svgdom::load(papki::FSFile(argv[1]));
	
	unsigned imWidth = 0;
	unsigned imHeight = 0;
	auto img = svgren::render(*dom, imWidth, imHeight);
	
	if(png.colorDepth() != Image::ColorDepth_e::RGBA){
		std::cout << "Error: PNG color depth is not RGBA: " << unsigned(png.colorDepth()) << std::endl;
		return 1;
	}
	
	if(imWidth != png.dim().x){
		std::cout << "Error: svg width (" << imWidth << ") did not match png width (" << png.dim().x << ")" << std::endl;
		return 1;
	}
	
	if(imHeight != png.dim().y){
		std::cout << "Error: svg height (" << imHeight << ") did not match png height (" << png.dim().y << ")" << std::endl;
		return 1;
	}
	
	if(img.size() != png.buf().size() / png.numChannels()){
		std::cout << "Error: svg pixel buffer size (" << img.size() << ") did not match png pixel buffer size(" << png.buf().size() / png.numChannels() << ")" << std::endl;
		return 1;
	}
	
	for(size_t i = 0; i != img.size(); ++i){
		std::uint32_t pixel =
				std::uint32_t(png.buf()[i * png.numChannels()]) |
				(std::uint32_t(png.buf()[i * png.numChannels() + 1]) << 8) |
				(std::uint32_t(png.buf()[i * png.numChannels() + 2]) << 16) |
				(std::uint32_t(png.buf()[i * png.numChannels() + 3]) << 24)
			;

		if(pixel != img[i]){
			std::cout << "Error: PNG pixel (0x" << std::hex << pixel << ") did not match SVG pixel (0x" << img[i] << ")" << std::endl;
			return 1;
		}
	}
	
	return 0;
}

