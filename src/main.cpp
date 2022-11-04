#include "converter.h"

//------------------------------------------------------------------------------

int main (int argc, char* argv[])
{
    // if (argc != 5)
    // {
    //     std::cout << std::endl
    //               << "Usage: transcoding -i input_file -o output_file."
    //               << std::endl << std::endl;
    //
    //     return 0;
    // }

    auto converter = std::make_unique<CConverter>();
    const char* input = nullptr;
    const char* output = nullptr;

    using namespace std::literals;
    for (int i = 0; i < argc; ++i)
    {
        if (argv[i] == "-i"sv)
        {
            input = argv[i+1];
        }
        else if (argv[i] == "-o"sv)
        {
            output = argv[i+1];
        }
    }

    std::cout << "input: " << input << std::endl;
    std::cout << "output: " << output << std::endl;
    std::cout << std::endl;

    // Check input file exist or not
    struct stat buffer;
    if (stat(input, &buffer) != 0)
    {
        fprintf(stderr, "ERROR: The input file not exist.\n");
        return false;
    }

    // Create output directory
    std::filesystem::path fullpath(output);
    std::filesystem::create_directories(fullpath.remove_filename());

    // Transcode the media file
    if (converter->convert(input, output) == false)
    {
        std::cout << "Transcoding failed" << std::endl;
        return 1;
    }

    std::cout << "Transcoding success" << std::endl;

    return 0;
}

//------------------------------------------------------------------------------
