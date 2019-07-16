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

inline int8_t DirectionFixMax(int8_t d) { return d >= 8 ? d - 8 : d; }
inline int8_t DirectionFixMin(int8_t d) { return d < 0 ? d + 8 : d; }
inline int8_t DirectionFix(int8_t d)
{
	while (d >= 8)
		d -= 8;
	while (d < 0)
		d += 8;
	return d;
}
inline bool isSame(uint8_t t, int8_t d) { return (t & (1 << d)) == (1 << d); }

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
			{1, 1},
			{1, 1},
			{0, 1},
			{0, 1},
			{0, 0},
			{0, 0},
			{1, 0},
			{1, 0},
		}; // 打表

		uint8_t *mask = new uint8_t[pic.cols * pic.rows]{};
		char color[7];
		color[6] = 0;
		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
			{
				auto p = pic.at<Vec4b>(y, x);
				if (p[3] &&
					mask[y * pic.cols + x] == 0 && // 未标记
					(pic.at<uint32_t>(y, x) != pic.at<uint32_t>(y, x + 1) ||
					 pic.at<uint32_t>(y, x) != pic.at<uint32_t>(y + 1, x) ||
					 pic.at<uint32_t>(y, x) != pic.at<uint32_t>(y, x - 1) ||
					 pic.at<uint32_t>(y, x) != pic.at<uint32_t>(y - 1, x)) // 任意一方是边缘
				)
				{
					// 找到一个起始点
					int xx = x;
					int yy = y;

					vector<Point> points;
					points.push_back({x : x, y : y}); // 添加第一个点
					int8_t lastDirection = 4;
					mask[yy * pic.cols + xx] |= (1 << lastDirection); // 起始点

					//开始搜索附近点
					while (1)
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
						uint8_t t = 0b00000000;
						for (uint8_t i = 0; i < 8; i++)
							t |= (pic.at<uint32_t>(yy, xx) == pic.at<uint32_t>(yy + pointAddByDirection[i].y, xx + pointAddByDirection[i].x)) << i;

						// 确定方向
						int8_t turnDirection = isSame(t, DirectionFixMax(lastDirection + 1)) ? -1 : 1;

						for (int8_t forDirection = lastDirection + 2 * turnDirection, endDirection = lastDirection + (8 + 1) * turnDirection; forDirection <= endDirection; forDirection += turnDirection) // 从决定的方向+2开始搜索下一个点 直到包括+8（原方向）结束
						{
							int8_t direction = DirectionFix(forDirection);

							if (direction % 2 == 1)											  // 偶数为边，奇数为角
								points.push_back({x : xx + pointAddByDirection2[direction].x, // 添加喵点
												  y : yy + pointAddByDirection2[direction].y});
							if (isSame(mask[yy * pic.cols + xx], direction)) // 遇到了起点
								goto closure;
							mask[yy * pic.cols + xx] |= (1 << direction);

							if (isSame(t, direction)) // direction方向上有相同色
							{
								// 下一个点坐标
								xx += pointAddByDirection[direction].x;
								yy += pointAddByDirection[direction].y;
								lastDirection = DirectionFixMax(direction + 4);

								goto nextPoint;
							}
						}

						goto closure;

					nextPoint:;
					}

				closure:

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

		// onlyfirst:

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