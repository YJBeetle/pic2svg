#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <png.h>
#include <getopt.h>

using namespace std;

void help()
{
	cerr << "Usage: png2svg <PNG_FILE> <SVG_FILE>" << '\n';
	exit(1);
}

class SvgMake
{
public:
	SvgMake()
	{
		memset(&png, 0, (sizeof png));
	}

	~SvgMake()
	{
		clear();
	}

	void clear()
	{
		png_image_free(&png);
		if (buffer)
		{
			free(buffer);
			buffer = nullptr;
		}
	}

	int openPng(string pngPath)
	{
		clear();
		png.version = PNG_IMAGE_VERSION;

		if (png_image_begin_read_from_file(&png, pngPath.c_str()) != 0)
		{
			png.format = PNG_FORMAT_RGBA;
			buffer = (png_bytep)malloc(PNG_IMAGE_SIZE(png));

			if (buffer != nullptr)
			{
				if (png_image_finish_read(&png, NULL /*background*/, buffer, 0 /*row_stride*/, NULL /*colormap*/) != 0)
				{
					cout << "Read PNG done." << endl;
				}
				else
				{
					cerr << "Read PNG file error." << endl;
					goto err;
				}
			}
			else
			{
				cerr << "malloc error." << endl;
				goto err;
			}
		}
		else
		{
			cerr << "Open PNG file error." << endl;
			goto err;
		}

		return 0;

	err:
		return 1;
	}

	int SaveToPng(string path)
	{
		if (png_image_write_to_file(&png, path.c_str(), 0 /*convert_to_8bit*/, buffer, 0 /*row_stride*/, NULL /*colormap*/) != 0)
		{
			/* The image has been written successfully. */
			return 0;
		}
		return 1;
	}

	void LimitColor(int colorQuantity)
	{
		struct ColorInfo
		{
			size_t usageCount = 0;
			double adjacentColorDistance = 512; // 512 = √[ 256 ^ 2 * 4 ]
			uint32_t mapTo;
		};

		unordered_map<uint32_t, ColorInfo> colorMap;

		// 统计 usageCount
		for (png_uint_32 y = 0; y < png.height; y++)
		{
			for (png_uint_32 x = 0; x < png.width; x++)
			{
				auto p = buffer + (png.width * 4 * y) + x * 4;
				uint32_t c = *((uint32_t *)p);
				auto t = colorMap.find(c);
				if (t != colorMap.end())
					t->second.usageCount++;
				else
					(colorMap[c] = ColorInfo()).usageCount = 1;
			}
		}

		// 统计 adjacentColorDistance
		for (auto color1 : colorMap)
		{
			for (auto color2 : colorMap)
			{
				uint8_t *colorRGBA_1 = (uint8_t *)(&(color1.first));
				uint8_t *colorRGBA_2 = (uint8_t *)(&(color2.first));

				double distance = sqrtf(powf(colorRGBA_1[0] - colorRGBA_2[0], 2) + pow(colorRGBA_1[1] - colorRGBA_2[1], 2) + pow(colorRGBA_1[2] - colorRGBA_2[2], 2) + pow(colorRGBA_1[3] - colorRGBA_2[3], 2));

				if (distance < color1.second.adjacentColorDistance)
					color1.second.adjacentColorDistance = distance;
			}
		}

		// colorQuantity

		// unordered_set<uint32_t> colorAdjacentColorDistanceSort;
	}

	void saveToSvgByPixel(string svgPath)
	{
		fstream svg;
		svg.open(svgPath, fstream::out);

		svg << R"(<?xml version="1.0" encoding="utf-8"?>)"
			<< R"(<!-- Generator: png2svg -->)";

		svg << R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 )" << png.width << ' ' << png.height << R"(">)";

		char color[7];
		color[6] = 0;
		for (png_uint_32 y = 0; y < png.height; y++)
		{
			for (png_uint_32 x = 0; x < png.width; x++)
			{
				auto p = buffer + (png.width * 4 * y) + x * 4;

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
	}

private:
	png_image png;
	png_bytep buffer = nullptr;
};

int main(int argC, char **argV)
{
	string pngPath;
	string svgPath;
	int limitColorQuantity = 32;

	int ch;
	while ((ch = getopt(argC, argV, "c:")) != -1)
		switch (ch)
		{
		case 'c':
			limitColorQuantity = stoi(optarg);
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
	cout << "limitColorQuantity: " << limitColorQuantity << '\n';

	SvgMake svgMake;

	cout << "Reading PNG..." << endl;
	svgMake.openPng(pngPath);

	cout << "Limit Color..." << endl;
	svgMake.LimitColor(limitColorQuantity);

#ifdef DEBUG
	string debug1png = pngPath + ".debug1.png";
	cout << "[DEBUG] save to" << debug1png << endl;
	svgMake.SaveToPng(debug1png);
#endif

	cout << "saveToSvgByPixel..." << endl;
	svgMake.saveToSvgByPixel(svgPath);
}