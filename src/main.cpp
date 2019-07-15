#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <vector>
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
		auto picRead = imread(path, IMREAD_UNCHANGED);
		if (picRead.channels() == 3)
			cvtColor(picRead, pic, CV_BGR2BGRA);
		else
		{
			pic = picRead;
			for (int y = 0; y < pic.rows; y++)
				for (int x = 0; x < pic.cols; x++)
					if (pic.at<Vec4b>(y, x)[3] == 0) // Alpha 为 0 的像素设置为黑色
						pic.at<Vec4b>(y, x)[0] = pic.at<Vec4b>(y, x)[1] = pic.at<Vec4b>(y, x)[2] = 0;
		}
	}

	void saveTo(string path)
	{
		imwrite(path, pic);
	}

	void limitColor(int colorQuantity)
	{
		Mat samples(pic.rows * pic.cols, 4, CV_32F);
		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
				for (int z = 0; z < 4; z++)
					samples.at<float>(y + x * pic.rows, z) = pic.at<Vec4b>(y, x)[z];

		Mat labels;
		Mat centers;
		kmeans(samples, colorQuantity, labels, TermCriteria(TermCriteria::COUNT | TermCriteria::EPS, 10000, 0.0001), 5 /* attempts */, KMEANS_PP_CENTERS, centers);

		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
			{
				int cluster_idx = labels.at<int>(y + x * pic.rows, 0);
				pic.at<Vec4b>(y, x)[0] = centers.at<float>(cluster_idx, 0);
				pic.at<Vec4b>(y, x)[1] = centers.at<float>(cluster_idx, 1);
				pic.at<Vec4b>(y, x)[2] = centers.at<float>(cluster_idx, 2);
				pic.at<Vec4b>(y, x)[3] = centers.at<float>(cluster_idx, 3);
			}
	}

	void saveToSvgByPixel(string svgPath)
	{
		fstream svg;
		svg.open(svgPath, fstream::out);

		// svg << R"(<?xml version="1.0" encoding="utf-8"?>)"
		// 	<< R"(<!-- Generator: pic2svg -->)";

		// svg << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << pic.cols << R"(" height=")" << pic.rows << R"(" viewBox="0 0 )" << pic.cols << ' ' << pic.rows << R"(">)" << endl;
		svg << R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 )" << pic.cols << ' ' << pic.rows << R"(">)" << endl;
		svg << "<title>pic2svg</title>" << endl;

		struct Point
		{
			int x;
			int y;
		};
		const Point pointAddByDirection[8] = {
			{1, 0},
			{1, 1},
			{0, 1},
			{-1, 1},
			{-1, 0},
			{-1, -1},
			{0, -1},
			{1, -1},
		}; // 打表
		const Point pointAddByDirection2[8] = {
			{1, 0},
			{1, 1},
			{0, 1},
			{-1, 1},
			{-1, 0},
			{-1, -1},
			{0, -1},
			{1, -1},
		}; // 打表

		bool *mask = new bool[pic.cols * pic.rows]{};
		char color[7];
		color[6] = 0;
		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
			{
				auto p = pic.at<Vec4b>(y, x);
				if (p[3] &&
					mask[y * pic.cols + x] == false && // 未标记
					(pic.at<uint32_t>(y, x) != pic.at<uint32_t>(y, x + 1) ||
					 pic.at<uint32_t>(y, x) != pic.at<uint32_t>(y + 1, x) ||
					 pic.at<uint32_t>(y, x) != pic.at<uint32_t>(y, x - 1) ||
					 pic.at<uint32_t>(y, x) != pic.at<uint32_t>(y - 1, x)) // 任意一方是边缘
				)
				{
					int xx = x;
					int yy = y;

					vector<Point> points;
					points.push_back({x : x, y : y}); // 添加第一个点

					uint8_t lastDirection = 0;

					for (size_t iiiiii = 0; iiiiii < 1000; iiiiii++)
					// while (1)
					{
						/*
							  7      0
							  ⬇      ⬇
							0b00000000
							 ___________
							| 5 | 6 | 7 |
							|---|---|---|
							| 4 | X | 0 |
							|---|---|---|
							| 3 | 2 | 1 |
							 -----------
						*/
						mask[yy * pic.cols + xx] = true;

						uint8_t t = 0b00000000;
						for (uint8_t i = 0; i < 8; i++)
						{
							t |= (pic.at<uint32_t>(yy, xx) == pic.at<uint32_t>(yy + pointAddByDirection[i].y, xx + pointAddByDirection[i].x)) << i;
						}

						for (uint8_t addDirection = 0; addDirection < 8; addDirection++)
						{
							uint8_t endDirection = lastDirection + addDirection;
							uint8_t direction = endDirection >= 8 ? endDirection - 8 : endDirection;

							if ((t & (1 << direction)) == (1 << direction)) // 下一个方向上有东西
							{
								// 添加喵点
								for (uint8_t nowDirection = lastDirection; nowDirection <= endDirection; nowDirection++)
								{
									uint8_t ddirection = nowDirection >= 8 ? nowDirection - 8 : nowDirection;
									points.push_back({x : xx + pointAddByDirection2[ddirection].x, y : yy + pointAddByDirection2[ddirection].y});
									// 	points.push_back({x : xx + 1, y : yy});		// 添加当前点的右上角坐标
									// 	points.push_back({x : xx + 1, y : yy + 1}); // 添加当前点的右下角坐标
									// 	points.push_back({x : xx, y : yy + 1});		// 添加当前点的左下角坐标
									// 	points.push_back({x : xx, y : yy});			// 添加当前点的左上角坐标
									// 	points.push_back({x : xx - 1, y : yy});		//
								}

								// 下一个点坐标
								xx += pointAddByDirection[direction].x;
								yy += pointAddByDirection[direction].y;
								lastDirection = direction;

								break;
							}
						}

						// if ((xx == x && yy == y) || mask[yy * pic.cols + xx])
						if (xx == x && yy == y)
							break;
					}

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

					svg << "<polygon";
					svg << " points=\"";
					for (auto point : points)
						svg << point.x << "," << point.y << " ";
					svg << "\"";
					svg << " fill=\"#" << color << "\"";
					if (p[3] != 0xFF)
						svg << " opacity=\"" << (float)p[3] / 0xFF << "\"";
					svg << "/>" << endl;

					// goto onlyfirst;	//debug: onlyfirst
				}
			}

			onlyfirst:

		svg << R"(</svg>)";

		free(mask);

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

	cout << "==========" << endl
		 << "pic: " << picPath << endl
		 << "svg: " << svgPath << endl
		 << "limitColorQuantity: " << limitColorQuantity << endl
		 << "==========" << endl;

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