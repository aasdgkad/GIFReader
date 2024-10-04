#include <string>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <bitset>

/*
    Overall this project is mostly complete, I haven't tested it with many Gif files and I am aware that it misses some features(quite a lot of them)
    List of things to add or look into:
    -Interlaced frames
    -Local Color Table (Also make it possible to handle cases where there is no global color table)
    -Correctly handle all disposal methods
    -Plain Text Extension
    -Application Extension
    -Correctly read the packed byte of the Logical Screen Descriptor
*/

namespace gr{
    class Color{
        public:
        Color(char r, char g, char b);
        Color() = default;
        ~Color() = default;
        Color& operator=(const Color& rhs);

        public:
        char r,g,b;
    };

    class Surface{
        public:
        Surface(int width, int height);
        Surface() = default;
        ~Surface();

        Color getPixel(int x, int y);
        void setPixel(int x, int y, Color c);

        private:
        int width, height;
        Color* pixel;
    };

    class Video{
        public:
        Video() = default;
        ~Video();

        Video(std::string fname);
        
        private:
        void lzwDecode();//decompresses whatever is inside compressedCata and puts the output in decompresseddata
        unsigned int getCode();//gets the next lzw code
        bool parseFrame();//looks for an image descriptor and then parses the frame
        //void convertFrames(); ?

        public:
        int nframes, width, height;
        //Surface* frame;
        std::vector<Surface*> vframe;// should I convert from this to Surface* ?

        private:
        std::ifstream file;
        Color* globalcolor;
        std::vector<unsigned char> compressedData;
        std::vector<unsigned char> decompresseddata;
        std::vector<std::vector<unsigned char>> compCodes;
        int nrbits;
        int bitoff;
        int lzwmincode;
    };
}



