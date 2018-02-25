//   GreKo chess engine
//   (c) 2002-2018 Vladimir Medvedev <vrm@bk.ru>
//   http://greko.su

#include "eval.h"
#include "eval_params.h"
#include "learn.h"
#include "moves.h"
#include "notation.h"
#include "search.h"
#include "utils.h"

static U32 g_t0 = 0;

void CoordinateDescentInfo(int param, int value, double y0Start, double y0, int t);
void RandomWalkInfo(double y0Start, double y0, int t);
double Regularization(const vector<int>& x);

void CoordinateDescent(const string& fenFile,
	vector<int>& x0,
	const vector<int>& params)
{
	InitEval(x0);
	double y0 = Predict(fenFile) + Regularization(x0);
	double y0Start = y0;

	cout << "Algorithm:     coordinate descent" << endl;
	cout << "Parameters:    " << NUM_PARAMS << endl;
	cout << "Initial value: " << left << y0Start << endl;

	std::ofstream timeLog;
	timeLog.open("timeLog.txt");
	if (timeLog.good())
		timeLog << 0 << "\t" << y0 << endl;

	for (size_t i = 0; i < params.size(); ++i)
	{
		int param = params[i];

		int t = (GetProcTime() - g_t0) / 1000;
		CoordinateDescentInfo(param, x0[param], y0Start, y0, t);

		int step = 1;
		while (step > 0)
		{
			vector<int> x1 = x0;
			x1[param] += step;

			if (x1[param] != x0[param])
			{
				InitEval(x1);
				double y1 = Predict(fenFile) + Regularization(x1);
				if (y1 < y0)
				{
					x0 = x1;
					y0 = y1;
					t = (GetProcTime() - g_t0) / 1000;
					CoordinateDescentInfo(param, x0[param], y0Start, y0, t);

					WriteParamsToFile(x0, "weights.txt");

					std::ofstream timeLog;
					timeLog.open("timeLog.txt", ios::app);
					if (timeLog.good())
						timeLog << t << "\t" << y0 << endl;

					continue;
				}
			}

			vector<int> x2 = x0;
			x2[param] -= step;

			if (x2[param] != x0[param])
			{
				InitEval(x2);
				double y2 = Predict(fenFile) + Regularization(x2);
				if (y2 < y0)
				{
					x0 = x2;
					y0 = y2;
					t = (GetProcTime() - g_t0) / 1000;
					CoordinateDescentInfo(param, x0[param], y0Start, y0, t);

					WriteParamsToFile(x0, "weights.txt");

					std::ofstream timeLog;
					timeLog.open("timeLog.txt", ios::app);
					if (timeLog.good())
						timeLog << t << "\t" << y0 << endl;

					continue;
				}
			}

			step /= 2;
		}
	}

	cout << endl << endl;
	InitEval(x0);
}
////////////////////////////////////////////////////////////////////////////////

void CoordinateDescentInfo(int param, int value, double y0Start, double y0, int t)
{
	int hh = t / 3600;
	int mm = (t - 3600 * hh) / 60;
	int ss = t - 3600 * hh - 60 * mm;

	cout << right;
	cout << setw(2) << setfill('0') << hh << ":";
	cout << setw(2) << setfill('0') << mm << ":";
	cout << setw(2) << setfill('0') << ss << " ";
	cout << setfill(' ');

	cout << left << "      " << setw(8) << y0 << " " << 100 * (y0 - y0Start) / y0Start << " % " << ParamNumberToName(param) << " = " << value << "          \r";
}
////////////////////////////////////////////////////////////////////////////////

int CountGames(const string& file)
{
	int games = 0;
	ifstream ifs(file.c_str());
	if (ifs.good())
	{
		string s;
		while (getline(ifs, s))
		{
			if (s.find("[Event") == 0)
				++games;
		}
	}
	return games;
}
////////////////////////////////////////////////////////////////////////////////

double ErrSq(const string& s, double K)
{
	char chRes = s[0];
	double result = 0;
	if (chRes == '1')
		result = 1;
	else if (chRes == '0')
		result = 0;
	else if (chRes == '=')
		result = 0.5;
	else
	{
		cout << "Illegal string: " << s << endl;
		return -1;
	}

	string fen = string(s.c_str() + 2);
	Position pos;
	if (!pos.SetFEN(fen))
	{
		cout << "ERR FEN: " << fen << endl;
		return -1;
	}

	EVAL e = Evaluate(pos, -INFINITY_SCORE, INFINITY_SCORE);
	if (pos.Side() == BLACK)
		e = -e;

	double prediction = 1. / (1. + exp(-e / K));
	double errSq = (prediction - result) * (prediction - result);

	return errSq;
}
////////////////////////////////////////////////////////////////////////////////

void GetRandomStep(vector<int>& step, const vector<int>& params)
{
	for (size_t i = 0; i < params.size(); ++i)
	{
		int index = params[i];
		step[index] = Rand32() % 3 - 1;
	}
}
////////////////////////////////////////////////////////////////////////////////

void MakeStep(vector<int>& x, const vector<int>& step)
{
	assert(x.size() == step.size());
	for (size_t i = 0; i < x.size(); ++i)
		x[i] += step[i];
}
////////////////////////////////////////////////////////////////////////////////

bool PgnToFen(const string& pgnFile,
	const string& fenFile,
	int minPly,
	int maxPly,
	int skipGames,
	int fensPerGame)
{
	RandSeed(time(0));

	ifstream ifs(pgnFile.c_str());
	if (!ifs.good())
	{
		cout << "Can't open file: " << pgnFile << endl << endl;
		return false;
	}

	ofstream ofs(fenFile.c_str(), ios::app);
	if (!ofs.good())
	{
		cout << "Can't open file: " << fenFile << endl << endl;
		return false;
	}

	string s;
	Position pos;
	vector<string> fens;
	int games = 0;

	cout << "Getting positions from " << pgnFile << endl;
	int total = CountGames(pgnFile);

	while (getline(ifs, s))
	{
		if (s.empty())
			continue;
		if (s.find("[Event") == 0)
		{
			++games;
			cout << "Processing games: " << games << " of " << total << "\r";
			if (games > skipGames)
			{
				pos.SetInitial();
				fens.clear();
			}
			continue;
		}
		if (s.find("[") == 0)
			continue;

		if (games <= skipGames)
			continue;

		vector<string> tokens;
		Split(s, tokens, ". ");

		for (size_t i = 0; i < tokens.size(); ++i)
		{
			string tk = tokens[i];
			if (tk == "1-0" || tk == "0-1" || tk == "1/2-1/2" || tk == "*")
			{
				char r = '?';
				if (tk == "1-0")
					r = '1';
				else if (tk == "0-1")
					r = '0';
				else if (tk == "1/2-1/2")
					r = '=';
				else
				{
					fens.clear();
					break;
				}

				if (!fens.empty())
				{
					for (int j = 0; j < fensPerGame; ++j)
					{
						int index = Rand32() % fens.size();
						ofs << r << " " << fens[index] << endl;
					}
				}

				/*
				for (size_t i = 0; i < fens.size(); ++i)
					ofs << r << " " << fens[i] << endl;
				*/

				fens.clear();
				break;
			}

			if (!CanBeMove(tk))
				continue;

			Move mv = StrToMove(tk, pos);
			if (mv && pos.MakeMove(mv))
			{
				if (pos.Ply() >= minPly && pos.Ply() <= maxPly)
				{
					if (!mv.Captured() && !mv.Promotion() && !pos.InCheck())
						fens.push_back(pos.FEN());
				}
			}
		}
	}
	cout << endl;
	return true;
}
////////////////////////////////////////////////////////////////////////////////

double Predict(const string& fenFile)
{
	const double K = 125;

	ifstream ifs(fenFile.c_str());
	int n = 0;
	double errSqSum = 0;
	string s;

	while (getline(ifs, s))
	{
		if (s.length() < 5)
			continue;

		double errSq = ErrSq(s, K);
		if (errSq < 0)
			continue;

		++n;
		errSqSum += errSq;
	}

	return sqrt(errSqSum / n);
}
////////////////////////////////////////////////////////////////////////////////

void RandomWalk(const string& fenFile,
	vector<int>& x0,
	int limitTimeInSec,
	bool simulatedAnnealing,
	const vector<int>& params)
{
	RandSeed(time(0));

	InitEval(x0);
	double y0 = Predict(fenFile) + Regularization(x0);
	double y0Start = y0;

	if (simulatedAnnealing)
		cout << "Algorithm:     simulated annealing" << endl;
	else
		cout << "Algorithm:     random walk" << endl;
	cout << "Time limit:    " << limitTimeInSec << endl;
	cout << "Initial value: " << left << y0Start << endl;

	int t = (GetProcTime() - g_t0) / 1000;
	std::ofstream timeLog;
	timeLog.open("timeLog.txt");
	if (timeLog.good())
		timeLog << t << "\t" << y0 << endl;

	int ssPrev = -1;
	while (1)
	{
		int t = (GetProcTime() - g_t0) / 1000;

		int hh = t / 3600;
		int mm = (t - 3600 * hh) / 60;
		int ss = t - 3600 * hh - 60 * mm;

		if (ss != ssPrev)
		{
			RandomWalkInfo(y0Start, y0, t);
			ssPrev = ss;
		}

		if (t >= limitTimeInSec)
		{
			cout << endl << endl;
			InitEval(x0);
			return;
		}

		vector<int> x = x0;
		vector<int> step;
		step.resize(x.size());

		GetRandomStep(step, params);
		MakeStep(x, step);

		bool transition = false;
		do
		{
			int t = (GetProcTime() - g_t0) / 1000;
			if (t >= limitTimeInSec)
			{
				cout << endl << endl;
				InitEval(x0);
				return;
			}

			InitEval(x);
			double y = Predict(fenFile) + Regularization(x);

			if (y <= y0)
				transition = true;
			else if (simulatedAnnealing)
			{
				double stage = 10 * t / limitTimeInSec;
				double T = 1.0e-4 * pow(10.0, -stage);
				double z = (y - y0) / T;
				double prob = exp(-z);
				int n = int(prob * 1000);
				int rand = Rand32() % 1000;
				if (rand <= n)
					transition = true;
				else
					transition = false;
			}
			else
				transition = false;

			if (transition)
			{
				x0 = x;
				y0 = y;

				WriteParamsToFile(x0, "weights.txt");
				RandomWalkInfo(y0Start, y0, t);
				
				std::ofstream timeLog;
				timeLog.open("timeLog.txt", ios::app);
				if (timeLog.good())
					timeLog << t << "\t" << y0 << endl;

				MakeStep(x, step);
			}
		}
		while (transition);
	}
}
////////////////////////////////////////////////////////////////////////////////

void RandomWalkInfo(double y0Start, double y0, int t)
{
	int hh = t / 3600;
	int mm = (t - 3600 * hh) / 60;
	int ss = t - 3600 * hh - 60 * mm;

	cout << right;
	cout << setw(2) << setfill('0') << hh << ":";
	cout << setw(2) << setfill('0') << mm << ":";
	cout << setw(2) << setfill('0') << ss << " ";

	cout << left << "      " <<
		setw(8) << y0 << " " <<
		100 * (y0 - y0Start) / y0Start << " % " <<
		"          \r";

	cout << setfill(' ');
}
////////////////////////////////////////////////////////////////////////////////

double Regularization(const vector<int>& x)
{
	const double lambda = 1e-10;
	double r = 0;
	for (size_t i = 0; i < x.size(); ++i)
		r += x[i] * x[i];
	return lambda * r / x.size();
}
////////////////////////////////////////////////////////////////////////////////

void SetStartLearnTime()
{
	g_t0 = GetProcTime();
}
////////////////////////////////////////////////////////////////////////////////
