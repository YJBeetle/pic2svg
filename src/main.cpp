#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <png.h>
#include <getopt.h>

using namespace std;

void help()
{
	cerr << "Usage: png2svg <PNG_FILE> <SVG_FILE>" << '\n';
	exit(1);
}

int main(int argC, char **argV)
{
	string pngPath;
	string svgPath;
	int limitColorSum = 10;

	int ch;
	while ((ch = getopt(argC, argV, "c:")) != -1)
		switch (ch)
		{
		case 'c':
			limitColorSum = stoi(optarg);
			break;
		case '?':
		default:
			help();
		}

	for (int index = optind; index < argC; index++)
		if (pngPath.empty())
			pngPath = argV[index];
		else if (svgPath.empty())
			svgPath = argV[index];

	if (pngPath.empty())
		help();

	if (svgPath.empty())
	{
		svgPath = pngPath.substr(0, pngPath.rfind(".")) + ".svg";
	}

	cout << "png: " << pngPath << '\n';
	cout << "svg: " << svgPath << '\n';
	cout << "limitColorSum: " << limitColorSum << '\n';

	png_image image;
	memset(&image, 0, (sizeof image));
	image.version = PNG_IMAGE_VERSION;

	if (png_image_begin_read_from_file(&image, pngPath.c_str()) != 0)
	{
		image.format = PNG_FORMAT_RGBA;
		png_bytep buffer = (png_bytep)malloc(PNG_IMAGE_SIZE(image));

		if (buffer != nullptr && png_image_finish_read(&image, NULL /*background*/, buffer, 0 /*row_stride*/, NULL /*colormap*/) != 0)
		{
			fstream svg;
			svg.open(svgPath, fstream::out);

			svg << R"(<?xml version="1.0" encoding="utf-8"?>)"
				<< R"(<!-- Generator: png2svg by pixel  -->)";

			svg << R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 )" << image.width << ' ' << image.height << R"(">)";

			char color[7];
			color[6] = 0;
			for (png_uint_32 y = 0; y < image.height; y++)
			{
				for (png_uint_32 x = 0; x < image.width; x++)
				{
					auto p = buffer + (image.width * 4 * y) + x * 4;

					if (p[3])
					{
						color[0] = p[0] >> 4;
						color[1] = p[0] & 0xF;
						color[2] = p[1] >> 4;
						color[3] = p[1] & 0xF;
						color[4] = p[2] >> 4;
						color[5] = p[2] & 0xF;
						color[0] = color[0] < 0xA ? color[0] + '0' : color[0] - 0xA + 'A';
						color[1] = color[1] < 0xA ? color[1] + '0' : color[1] - 0xA + 'A';
						color[2] = color[2] < 0xA ? color[2] + '0' : color[2] - 0xA + 'A';
						color[3] = color[3] < 0xA ? color[3] + '0' : color[3] - 0xA + 'A';
						color[4] = color[4] < 0xA ? color[4] + '0' : color[4] - 0xA + 'A';
						color[5] = color[5] < 0xA ? color[5] + '0' : color[5] - 0xA + 'A';
						svg << R"(<rect width="1" height="1" )";
						svg << "x=\"" << x << "\" ";
						svg << "y=\"" << y << "\" ";
						svg << "fill=\"#" << color << "\" ";
						if (p[3] != 0xFF)
							svg << "opacity=\"" << (float)p[3] / 0xFF << "\" ";
						svg << "/>";
					}
				}
			}

			svg << R"(</svg>)";

			svg.close();
			png_image_free(&image);
			free(buffer);

			cout << "done." << endl;
		}
		else
		{
			if (buffer == nullptr)
				png_image_free(&image);
			else
				free(buffer);

			cerr << "Read PNG file error." << endl;
		}
	}
	else
		cerr << "Open PNG file error." << endl;
}