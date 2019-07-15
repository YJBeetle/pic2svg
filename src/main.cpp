#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <getopt.h>

using namespace std;
using namespace cv;

void help()
{
	cerr << "Usage: pic2svg <PICTURE_FILE> <SVG_FILE>" << '\n';
	exit(1);
}

class SvgMake
{
public:
	SvgMake()
	{
	}

	~SvgMake()
	{
	}

	void openPic(string path)
	{
		pic = imread(path,IMREAD_UNCHANGED);
	}

	void saveTo(string path)
	{
		imwrite(path, pic);
	}

	void limitColor(int colorQuantity)
	{
		Mat samples(pic.rows * pic.cols, 3, CV_32F);
		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
				for (int z = 0; z < 3; z++)
					samples.at<float>(y + x * pic.rows, z) = pic.at<Vec4b>(y, x)[z];

		int clusterCount = colorQuantity;
		Mat labels;
		int attempts = 5;
		Mat centers;
		kmeans(samples, clusterCount, labels, TermCriteria(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 10000, 0.0001), attempts, KMEANS_PP_CENTERS, centers);

		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
			{
				int cluster_idx = labels.at<int>(y + x * pic.rows, 0);
				pic.at<Vec4b>(y, x)[0] = centers.at<float>(cluster_idx, 0);
				pic.at<Vec4b>(y, x)[1] = centers.at<float>(cluster_idx, 1);
				pic.at<Vec4b>(y, x)[2] = centers.at<float>(cluster_idx, 2);
			}
	}

	void saveToSvgByPixel(string svgPath)
	{
		fstream svg;
		svg.open(svgPath, fstream::out);

		svg << R"(<?xml version="1.0" encoding="utf-8"?>)"
			<< R"(<!-- Generator: pic2svg -->)";

		svg << R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 )" << pic.cols << ' ' << pic.rows << R"(">)";

		char color[7];
		color[6] = 0;
		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
			{
				auto p = pic.at<Vec4b>(y, x);

				if (p[3])
				{
					color[0] = p[2] >> 4;
					color[1] = p[2] & 0xF;
					color[2] = p[1] >> 4;
					color[3] = p[1] & 0xF;
					color[4] = p[0] >> 4;
					color[5] = p[0] & 0xF;
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

		svg << R"(</svg>)";

		svg.close();
	}

private:
	Mat pic;
};

int main(int argC, char **argV)
{
	string picPath;
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
		if (picPath.empty())
			picPath = argV[index];
		else if (svgPath.empty())
			svgPath = argV[index];

	if (picPath.empty())
		help();

	if (svgPath.empty())
	{
		svgPath = picPath.substr(0, picPath.rfind(".")) + ".svg";
	}

	cout << "pic: " << picPath << '\n';
	cout << "svg: " << svgPath << '\n';
	cout << "limitColorQuantity: " << limitColorQuantity << '\n';

	SvgMake svgMake;

	cout << "Reading picture..." << endl;
	svgMake.openPic(picPath);

	cout << "Limit Color..." << endl;
	svgMake.limitColor(limitColorQuantity);

#ifdef DEBUG
	string debug1pic = picPath + ".debug1.png";
	cout << "[DEBUG] save to: " << debug1pic << endl;
	svgMake.saveTo(debug1pic);
#endif

	cout << "saveToSvgByPixel..." << endl;
	svgMake.saveToSvgByPixel(svgPath);
}