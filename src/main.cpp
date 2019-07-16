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
		unordered_map<uint32_t, vector<vector<Point>>> pointsArrayByColors;
		bool *mask = new bool[pic.cols * pic.rows]{};
		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
			{
				uint32_t p = pic.at<uint32_t>(y, x);
				if ((p & 0xFF000000) &&				   // Aplha不为空
					mask[y * pic.cols + x] == false && // 未标记
					(p != pic.at<uint32_t>(y, x + 1) ||
					 p != pic.at<uint32_t>(y + 1, x) ||
					 p != pic.at<uint32_t>(y, x - 1) ||
					 p != pic.at<uint32_t>(y - 1, x)) // 任意一方是边缘
				)
				{
					// 找到一个起始点
					int xx = x;
					int yy = y;
					unique_ptr<uint8_t[]> maskNow(new uint8_t[pic.cols * pic.rows]{});
					vector<Point> points;

					points.push_back({x : x, y : y});		 // 添加第一个点
					int8_t lastDirection = 4;				 // 起始方向
					maskNow[yy * pic.cols + xx] |= (1 << 4); // 起始点
					maskNow[yy * pic.cols + xx] |= (1 << 5); // 起始点
					mask[yy * pic.cols + xx] = true;

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

						// 确定搜索旋转方向
						int8_t turnDirection = isSame(t, DirectionFixMax(lastDirection + 1)) ? -1 : 1;

						for (int8_t forDirection = lastDirection + 2 * turnDirection, endDirection = lastDirection + (8) * turnDirection; forDirection <= endDirection; forDirection += turnDirection) // 从决定的方向+2开始搜索下一个点 直到包括+8（原方向）结束
						{
							int8_t direction = DirectionFix(forDirection);

							if (isSame(maskNow[yy * pic.cols + xx], direction)) // 遇到了起点
								goto closure;
							maskNow[yy * pic.cols + xx] |= (1 << direction);
							mask[yy * pic.cols + xx] = true;

							if (direction % 2 == 1)											  // 偶数为边，奇数为角
								points.push_back({x : xx + pointAddByDirection2[direction].x, // 添加喵点
												  y : yy + pointAddByDirection2[direction].y});

							if (isSame(t, direction)) // direction方向上有相同色
							{
								// 下一个点坐标
								xx += pointAddByDirection[direction].x;
								yy += pointAddByDirection[direction].y;
								lastDirection = DirectionFixMax(direction + 4);

								goto nextPoint;
							}
						}

						goto ignore;

					nextPoint:;
					}

				closure:

					pointsArrayByColors[p].push_back(std::move(points));

				ignore:;
				}
			}
		free(mask);

		fstream svg;
		svg.open(svgPath, fstream::out);
		char color[7];
		color[6] = 0;

		svg << R"(<?xml version="1.0" encoding="utf-8"?>)" << endl
			<< R"(<!-- Generator: pic2svg -->)" << endl
			// << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << pic.cols << R"(" height=")" << pic.rows << R"(" viewBox="0 0 )" << pic.cols << ' ' << pic.rows << R"(">)" << endl
			<< R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 )" << pic.cols << ' ' << pic.rows << R"(">)" << endl
			<< "<title>pic2svg</title>" << endl;

		for (auto pointsArray : pointsArrayByColors)
		{
			uint32_t p = pointsArray.first;
			color[0] = (p & 0xF00000) >> 20;
			color[1] = (p & 0x0F0000) >> 16;
			color[2] = (p & 0x00F000) >> 12;
			color[3] = (p & 0x000F00) >> 8;
			color[4] = (p & 0x0000F0) >> 4;
			color[5] = (p & 0x00000F) >> 0;
			color[0] = color[0] < 0xA ? color[0] + '0' : color[0] - 0xA + 'A';
			color[1] = color[1] < 0xA ? color[1] + '0' : color[1] - 0xA + 'A';
			color[2] = color[2] < 0xA ? color[2] + '0' : color[2] - 0xA + 'A';
			color[3] = color[3] < 0xA ? color[3] + '0' : color[3] - 0xA + 'A';
			color[4] = color[4] < 0xA ? color[4] + '0' : color[4] - 0xA + 'A';
			color[5] = color[5] < 0xA ? color[5] + '0' : color[5] - 0xA + 'A';

			svg << "<path d=\"";

			for (auto points : pointsArray.second)
			{
				svg << "M" << points.back().x << "," << points.back().y;
				points.pop_back();
				for (auto point : points)
					svg << "L" << point.x << "," << point.y;
				svg << "Z ";
			}

			svg << "\"";
			svg << " fill=\"#" << color << "\"";
			if ((p >> 24) != 0xFF)
				svg << " opacity=\"" << (float)(p >> 24) / 0xFF << "\"";
			svg << "/>" << endl;
		}

		svg << R"(</svg>)";

		svg.close();
	}

private:
	Mat pic;

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