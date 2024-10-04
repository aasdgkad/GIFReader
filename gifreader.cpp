#include "gifreader.hpp"

gr::Color::Color(char r, char g, char b) : r(r), g(g), b(b)
{
}

gr::Color& gr::Color::operator=(const gr::Color& rhs){
    this->r = rhs.r;
    this->g = rhs.g;
    this->b = rhs.b;
    return *this;
}

gr::Surface::Surface(int width, int height) : width(width), height(height)
{
    pixel = new Color[width * height];
}

gr::Color gr::Surface::getPixel(int x, int y)
{
    return pixel[y * width + x];
}

void gr::Surface::setPixel(int x, int y, Color c)
{
    pixel[y * width + x] = c;
}

gr::Surface::~Surface(){
    delete[] pixel;
}

// This guy gets the current LZW code and also prepares the variables for the next code reading, said codes are read from right to left and have variable lengths (3-12 bits)

unsigned int gr::Video::getCode()
{
    unsigned int index = bitoff / 8;
    unsigned int rem = bitoff % 8;

    // We read the first 3 bytes from right to left
    unsigned int value = ((compressedData[index] >> rem) | (compressedData[index + 1] << (8 - rem)) | (compressedData[index + 2] << (16 - rem))) & ((1L << nrbits) - 1);

    bitoff += nrbits;

    return value;
}

void gr::Video::lzwDecode()
{
    int lcode = getCode();
    int ccode = getCode();

    while (true)
    {
        std::vector<unsigned char> newCompCode;

        // If it isn't a composed code then just take the byte itself
        if (lcode < 1L << lzwmincode)
        {
            decompresseddata.push_back(lcode);
            newCompCode.push_back(lcode);
        }
        // This is a composed code, so you need to look it up in the table and write the bytes it represents
        else if ((lcode != 1L << lzwmincode) && (lcode != ((1 << lzwmincode) + 1)))
        {
            int ni = lcode - ((1 << lzwmincode) + 2);
            for (int i = 0; i < compCodes[ni].size(); i++)
            {
                decompresseddata.push_back(compCodes[ni][i]);
                newCompCode.push_back(compCodes[ni][i]);
            }
        }

        // This code is the clear code so clear your table and reset the size
        if (ccode == 1 << lzwmincode)
        {
            compCodes.clear();
            nrbits = lzwmincode + 1;
        }
        // This code is the End Of Information code so stop everything and reset variables so that you prepare for the next frame, you are done
        else if (ccode == ((1 << lzwmincode) + 1))
        {
            return;
        }
        // If the current code and the next code are compatible for a new composed code (They are not clear or EOI codes) then add this composed code to your table
        else if (newCompCode.size() > 0)
        {
            if (ccode < 1L << lzwmincode)
            {
                newCompCode.push_back(ccode);
            }
            else if (ccode <= ((1 << lzwmincode) + 1 + compCodes.size()))
            {
                int ni = ccode - ((1 << lzwmincode) + 2);
                newCompCode.push_back(compCodes[ni][0]);
            }
            else
            {
                newCompCode.push_back(newCompCode[0]);
            }
            compCodes.push_back(newCompCode);

            // We add 1 bit to the size of our codes when we reach the current limit;
            if (compCodes.size() + ((1 << lzwmincode) + 1) == ((1L << nrbits) - 1))
            {
                if (nrbits != 12)
                {
                    nrbits++;
                }
            }
        }

        // Set the next code as the current code and get a new next code to prepare for the next iteration
        lcode = ccode;
        ccode = getCode();
    }
}

gr::Video::Video(std::string fname) : nframes(0), bitoff(0), file(fname, std::ios::binary)
{

    std::cout << "Version: ";
    char gif[3];
    file.read(gif, 3);
    for (int i = 0; i < 3; i++)
    {
        std::cout << gif[i];
    }

    char ver[3];
    file.read(ver, 3);
    for (int i = 0; i < 3; i++)
    {
        std::cout << ver[i];
    }
    std::cout << std::endl;

    // Here we are reading the dimensions
    short int d[2];
    file.read((char *)d, 2 * sizeof(short int));
    this->width = d[0];
    this->height = d[1];
    //std::cout << "Width: " << d[0] << " Height: " << d[1] << std::endl;

    // Packed byte that contains the size of the global color table and other information that I apparently did not deem valuable
    char packed;
    file.read(&packed, 1);
    int sizecolor = 0b111 & packed;
    int ncolors = (1L << (sizecolor + 1));
    //std::cout << "Size of global color table:" << ncolors << std::endl;

    // The background color
    char bgcolor;
    file.read(&bgcolor, 1);
    std::cout << "The background color index:" << int(bgcolor) << std::endl;

    char pxratio;
    file.read(&pxratio, 1);
    std::cout << "Pixel ratio:" << int(pxratio) << std::endl
              << std::endl;

    globalcolor = new Color[ncolors];
    for (int i = 0; i < ncolors; i++)
    {
        file.read(&(globalcolor[i].r), 1);
        file.read(&(globalcolor[i].g), 1);
        file.read(&(globalcolor[i].b), 1);
    }

    // The gce block separator I believe
    char god;
    file.read(&god, 1);
    //std::cout << god << std::endl;

    // Here we are reading and decompressing frames
    while(parseFrame());
    
    //std::cout << "Number of frames:" << nframes << std::endl;
}

bool gr::Video::parseFrame()
{
    // debug purpose please delete
    static int c = 0;
    c++;


    this->decompresseddata.clear();
    this->compressedData.clear();
    this->compCodes.clear();
    this->bitoff = 0;

    int transpindex = -1;
    int dispmode;

    unsigned char pleasegod;
    do
    {
        file.read((char*)&pleasegod, 1);
        if(pleasegod == 0x21){
            file.read((char*)&pleasegod, 1);
            if(pleasegod == 0xF9){
                //It means this is actually a GCE block
                std::cout << "GCE BLOCK\n";
                char gcesize;
                file.read(&gcesize, 1);
                std::cout << "size:" << (int)gcesize << std::endl;
                char gcepack;
                file.read(&gcepack, 1);
                bool trans = gcepack & 0b1;
                dispmode = (gcepack >> 2) & 0b111;
                std::cout << "transparency:" << trans << std::endl;
                std::cout << "dispmode:" << dispmode << std::endl;
                short int delay;
                file.read((char*)&delay, 2);
                std::cout << "delay time:" << delay/100.0f << "seconds\n";
                unsigned char transindex;
                file.read((char*)&transindex, 1);
                if(trans)
                    transpindex = transindex;
                std::cout << "transparency index:" << (int)transindex << std::endl << std::endl;
            }
        }
        if(!file.good()){
            return false;
        } 
    } while (pleasegod != 0x2C);

    std::cout << pleasegod << std::endl;

    unsigned short int left, right, width, height;
    char idpacked;
    file.read((char *)&left, 2);
    file.read((char *)&right, 2);
    file.read((char *)&width, 2);
    file.read((char *)&height, 2);
    file.read((char *)&idpacked, 1);

    std::cout << "frame:" << c << " l: " << left << " r:" << right << " w:" << width << " h:" << height << std::endl;
    std::cout << "local color table:" << bool((0b1 << 7) & idpacked) << " interlaced:" << bool((0b1 << 6) & idpacked) << std::endl;

    char lzwmin;

    file.read(&lzwmin, 1);
    std::cout << "Starting bits: " << int(lzwmin) << std::endl;

    unsigned char dbsize;
    while (true)
    {
        file.read((char *)&dbsize, 1);
        if (dbsize == 0)
            break;
        for (int i = 0; i < dbsize; i++)
        {
            unsigned char databyte;
            file.read((char *)&databyte, 1);
            compressedData.push_back(databyte);
        }
    }

    std::cout << "size of com:" << compressedData.size() << std::endl;

    this->lzwmincode = lzwmin;
    this->nrbits = lzwmin + 1;
    lzwDecode();

    std::cout << "size of dec:" << decompresseddata.size() << std::endl;

    Surface* newframe = new Surface(width, height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (y * width + x < decompresseddata.size()){
                if((decompresseddata[y * width + x] == transpindex) && (nframes > 0) && (dispmode == 1)){
                    newframe->setPixel(x, y, vframe[nframes-1]->getPixel(x,y));
                }
                else{
                    newframe->setPixel(x, y, globalcolor[decompresseddata[y * width + x]]);
                }
            }
            else
            {
                newframe->setPixel(x, y, Color(255, 0, 0));
            }
        }
    }
    this->vframe.push_back(newframe);
    this->nframes++;

    return true;
}

gr::Video::~Video(){
    for(int i = 0; i < vframe.size(); i++){
        delete vframe[i]; 
    }
}
