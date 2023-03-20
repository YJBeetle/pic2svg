#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <list>
#include <vector>
#include <opencv2/opencv.hpp>
#include <getopt.h>

inline int8_t directionFix(int8_t d) { return d & 0b111; }
inline bool isSame(uint8_t t, int8_t d) { return (t & (1 << d)) == (1 << d); }

using namespace std;
using namespace cv;

void help()
{
	cerr << "Usage: pic2svg picture_file [svg_file] [-c limit_color_quantity] [-a]" << endl
		 << "\t-c\tSet limit color quantity" << endl
		 << "\t-a\tAdjacency or overlap (Default overlap)" << endl;
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
			cvtColor(picRead, pic, COLOR_BGR2BGRA);
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

		colors = Mat(centers.rows, 1, CV_8UC4);
		for (int row = 0; row < centers.rows; row++)
			for (int c = 0; c < 4; c++)
				colors.at<Vec4b>(row, 0)[c] = centers.at<float>(row, c);

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

	void saveToSvgByPixel(string svgPath, bool adjacencyMode = false)
	{
		unordered_map<uint32_t, vector<list<Point>>> pointsArrayByColors; // 每个颜色的点数据
		unordered_map<uint32_t, size_t> colorsIndex;					  // 颜色映射到序号
		vector<vector<uint8_t>> masks;									  // 每个颜色的标记数据
		// 初始化
		for (size_t ci = 0; ci < colors.rows; ci++)
		{
			colorsIndex.insert({colors.at<uint32_t>(ci, 0), ci});
			masks.push_back(std::vector<uint8_t>(pic.cols * pic.rows));
		}
		// 遍历像素
		for (int y = 0; y < pic.rows; y++)
			for (int x = 0; x < pic.cols; x++)
			{
				int ci = colorsIndex[pic.at<uint32_t>(y, x)]; // 当前颜色序号

				if (
					(pic.at<Vec4b>(y, x)[3]) &&									  // Aplha不为空
					(y == 0 || pic.at<Vec4b>(y, x) != pic.at<Vec4b>(y - 1, x)) && // 上方是边缘
					(masks[ci][y * pic.cols + x] & (1 << 6)) == 0				  // 且这个颜色上方未被标记
				)
				{
					// 找到一个起始点
					int xx = x;
					int yy = y;
					list<Point> points;
					std::vector<uint8_t> maskNow(pic.cols * pic.rows);

					int8_t lastDirection = 5; // 起始方向

					// 开始搜索附近点
					while (1)
					{
						/*
							  7      0
							  ↓      ↓
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
							t |= (xx + pointAddByDirection[i].x >= 0 &&
								  xx + pointAddByDirection[i].x < pic.cols &&
								  yy + pointAddByDirection[i].y >= 0 &&
								  yy + pointAddByDirection[i].y < pic.rows &&																  // 没有超界
								  pic.at<uint32_t>(yy, xx) == pic.at<uint32_t>(yy + pointAddByDirection[i].y, xx + pointAddByDirection[i].x)) // 是相同色
								 << i;

						// 如果上下左右都被包围则放弃这条线
						if ((t & 0b01010101) == 0b01010101)
							goto ignore;

						// 确定搜索旋转方向
						int8_t turnDirection = isSame(t, directionFix(lastDirection + 1)) ? -1 : 1;

						for (int8_t forDirection = lastDirection + 1 * turnDirection, endDirection = lastDirection + (8 + 1) * turnDirection;
							 forDirection * turnDirection <= endDirection * turnDirection;
							 forDirection += turnDirection) // 从决定的方向+1开始搜索下一个点 直到一圈结束
						{
							int8_t direction = directionFix(forDirection);

							if (isSame(maskNow[yy * pic.cols + xx], direction)) // 遇到了起点
								goto closure;
							maskNow[yy * pic.cols + xx] |= (1 << direction);
							masks[ci][yy * pic.cols + xx] |= (1 << direction);

							if (adjacencyMode)
							{
								if (direction % 2 == 1) // 偶数为边，奇数为角
								{
									// 当是奇数时
									const Point pointABD[8] = {{NAN, NAN}, {1, 1}, {NAN, NAN}, {0, 1}, {NAN, NAN}, {0, 0}, {NAN, NAN}, {1, 0}};
									points.push_back({.x = xx + pointABD[direction].x,
													  .y = yy + pointABD[direction].y});
								}
							}
							else
							{
								if (direction % 2 == 1) // 偶数为边，奇数为角
								{
									// 当是奇数时
									auto directionN = directionFix(direction + 1);															 // 上一个的方向序号
									auto directionP = directionFix(direction - 1);															 // 下一个的方向序号
									int xThis = xx + pointAddByDirection[direction].x;														 // 当前方向的外侧坐标X
									int yThis = yy + pointAddByDirection[direction].y;														 // 当前方向的外侧坐标Y
									int xNext = xx + pointAddByDirection[directionN].x;														 // 上一个方向的外侧坐标X
									int yNext = yy + pointAddByDirection[directionN].y;														 // 上一个方向的外侧坐标Y
									int xPrev = xx + pointAddByDirection[directionP].x;														 // 下一个方向的外侧坐标X
									int yPrev = yy + pointAddByDirection[directionP].y;														 // 下一个方向的外侧坐标Y
									auto lThis = pic.at<Vec4b>(yThis, xThis)[3] == 0xff && colorsIndex[pic.at<uint32_t>(yThis, xThis)] > ci; // 当前方向不透明且压住
									auto lNext = pic.at<Vec4b>(yNext, xNext)[3] == 0xff && colorsIndex[pic.at<uint32_t>(yNext, xNext)] > ci; // 上一个方向不透明且压住
									auto lPrev = pic.at<Vec4b>(yPrev, xPrev)[3] == 0xff && colorsIndex[pic.at<uint32_t>(yPrev, xPrev)] > ci; // 下一个方向不透明且压住

									if (lThis == true && lNext == true && lPrev == true)
									{
										const Point pointABD[8] = {{NAN, NAN}, {1.5, 1.5}, {NAN, NAN}, {-0.5, 1.5}, {NAN, NAN}, {-0.5, -0.5}, {NAN, NAN}, {1.5, -0.5}};
										points.push_back({.x = xx + pointABD[direction].x,
														  .y = yy + pointABD[direction].y});
									}
									else if (lThis == true && lNext == false && lPrev == true)
									{
										const Point pointABD[8] = {{NAN, NAN}, {1.5, 1}, {NAN, NAN}, {0, 1.5}, {NAN, NAN}, {-0.5, 0}, {NAN, NAN}, {1, -0.5}};
										points.push_back({.x = xx + pointABD[direction].x,
														  .y = yy + pointABD[direction].y});
									}
									else if (lThis == true && lNext == true && lPrev == false)
									{
										const Point pointABD[8] = {{NAN, NAN}, {1, 1.5}, {NAN, NAN}, {-0.5, 1}, {NAN, NAN}, {0, -0.5}, {NAN, NAN}, {1.5, 0}};
										points.push_back({.x = xx + pointABD[direction].x,
														  .y = yy + pointABD[direction].y});
									}
									else
									{
										const Point pointABD[8] = {{NAN, NAN}, {1, 1}, {NAN, NAN}, {0, 1}, {NAN, NAN}, {0, 0}, {NAN, NAN}, {1, 0}};
										points.push_back({.x = xx + pointABD[direction].x,
														  .y = yy + pointABD[direction].y});
									}
								}
								else
								{
									// 当是偶数时
									int xThis = xx + pointAddByDirection[direction].x;	  // 当前方向的外侧坐标
									int yThis = yy + pointAddByDirection[direction].y;	  // 当前方向的外侧坐标
									if (pic.at<Vec4b>(yThis, xThis)[3] == 0xff &&		  // 若相邻的是不透明的
										colorsIndex[pic.at<uint32_t>(yThis, xThis)] > ci) // 且被相邻颜色压住的
									{
										const Point pointABD[8] = {{1.5, 0.5}, {NAN, NAN}, {0.5, 1.5}, {NAN, NAN}, {-0.5, 0.5}, {NAN, NAN}, {0.5, -0.5}, {NAN, NAN}};
										points.push_back({.x = xx + pointABD[direction].x,
														  .y = yy + pointABD[direction].y});
									}
								}
							}

							if (isSame(t, direction)) // direction方向上有相同色
							{
								// 下一个点坐标
								xx += pointAddByDirection[direction].x;
								yy += pointAddByDirection[direction].y;
								lastDirection = directionFix(direction + 4);

								goto nextPoint;
							}
						}

						goto ignore;

					nextPoint:;
					}

				closure:
					// 闭合
					if (points.size())
						pointsArrayByColors[pic.at<uint32_t>(y, x)].push_back(std::move(points));

				ignore:;
				}
			}

		// 删除直线上的不需要的点
		for (auto &pointsArray : pointsArrayByColors)
		{
			for (auto &points : pointsArray.second)
			{
				// 删除相邻重合点
				{
					Point last = points.front();
					Point now = {-1, -1};
					for (auto point = ++points.cbegin(); point != points.cend();)
					{
						now = *point;
						if (now.x == last.x && now.y == last.y) // 重合
						{
							points.erase(point++);
							last = now;
						}
						else
						{
							last = now;
							point++;
						}
					}
					// 结尾处理
					last = points.front();
					now = points.back();
					if (last.x == now.x && last.y == now.y)
						points.pop_back();
				}

				// 删除三点一线
				{
					Point lastlast = points.front();
					Point last = *(++points.cbegin());
					Point now = {-1, -1};
					for (auto point = ++ ++points.cbegin(); point != points.cend(); point++)
					{
						now = *point;
						if ((now.x * last.y - now.y * last.x) + (last.x * lastlast.y - last.y * lastlast.x) + (lastlast.x * now.y - lastlast.y * now.x) == 0) // 三点一线
						{
							points.erase((--point)++);
							last = now;
						}
						else
						{
							lastlast = last;
							last = now;
						}
					}
					// 结尾处理
					lastlast = *(++points.crbegin());
					last = points.back();
					now = points.front();
					if ((now.x * last.y - now.y * last.x) + (last.x * lastlast.y - last.y * lastlast.x) + (lastlast.x * now.y - lastlast.y * now.x) == 0)
						points.pop_back();
					// 结尾处理
					lastlast = *(++points.cbegin());
					last = points.front();
					now = points.back();
					if ((now.x * last.y - now.y * last.x) + (last.x * lastlast.y - last.y * lastlast.x) + (lastlast.x * now.y - lastlast.y * now.x) == 0)
						points.pop_front();
				}
			}
		}

		fstream svg;
		svg.open(svgPath, fstream::out);
		char color[7];
		color[6] = 0;

		svg << R"(<?xml version="1.0" encoding="utf-8"?>)" << endl
			<< R"(<!-- Generator: pic2svg -->)" << endl
			<< R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 )" << pic.cols << ' ' << pic.rows << R"(">)" << endl;

		for (size_t ci = 0; ci < colors.rows; ci++)
		{
			auto c = colors.at<uint32_t>(ci, 0);
			auto &pointsArray = pointsArrayByColors[c];
			color[0] = (c & 0xF00000) >> 20;
			color[1] = (c & 0x0F0000) >> 16;
			color[2] = (c & 0x00F000) >> 12;
			color[3] = (c & 0x000F00) >> 8;
			color[4] = (c & 0x0000F0) >> 4;
			color[5] = (c & 0x00000F) >> 0;
			color[0] = color[0] < 0xA ? color[0] + '0' : color[0] - 0xA + 'A';
			color[1] = color[1] < 0xA ? color[1] + '0' : color[1] - 0xA + 'A';
			color[2] = color[2] < 0xA ? color[2] + '0' : color[2] - 0xA + 'A';
			color[3] = color[3] < 0xA ? color[3] + '0' : color[3] - 0xA + 'A';
			color[4] = color[4] < 0xA ? color[4] + '0' : color[4] - 0xA + 'A';
			color[5] = color[5] < 0xA ? color[5] + '0' : color[5] - 0xA + 'A';

			svg << "<path d=\"";

			for (auto &points : pointsArray)
			{
				auto point = points.begin();
				svg << "M" << point->x << "," << point->y, point++;
				while (point != points.end())
					svg << "L" << point->x << "," << point->y, point++;
				svg << "Z ";
			}

			svg << "\"";
			svg << " fill=\"#" << color << "\"";
			if ((c >> 24) != 0xFF)
				svg << " opacity=\"" << (float)(c >> 24) / 0xFF << "\"";
			svg << "/>" << endl;
		}

		svg << R"(</svg>)";

		svg.close();
	}

private:
	Mat pic;
	Mat colors;

	struct Point
	{
		float x;
		float y;
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
};

int main(int argC, char **argV)
{
	string picPath;
	string svgPath;
	int limitColorQuantity = 32;
	bool adjacencyMode = false;

	int ch;
	while ((ch = getopt(argC, argV, "c:a")) != -1)
		switch (ch)
		{
		case 'c':
			limitColorQuantity = stoi(optarg);
			break;
		case 'a':
			adjacencyMode = true;
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
		 << "adjacencyMode: " << (adjacencyMode ? "on" : "off") << endl
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
	svgMake.saveToSvgByPixel(svgPath, adjacencyMode);
}