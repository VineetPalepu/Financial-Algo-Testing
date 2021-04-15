#include <iostream>
#include <random>

#include "Matrix.h"
#include "Timer.h"
#include "Financial.h"

using namespace std;
using namespace MatrixLibrary;


Matrix<double> algo(Matrix<double> percent_change_data, double multiplier, double out_multiplier, double pullout_thresh, double backin_thresh, int lookback_days);
Matrix<double> net_growth(Matrix<double> percent_change_data);

/*
tuple<Matrix<double>, Matrix<double>> getDataFromCSV(string filepath);

vector<int> getSplitIndexes(int length, int groups);
vector<double> getParamRange(double lowVal, double highVal, int num);

*/

int main()
{ 
    string folder = "../data/";
    Matrix<double> sp500 = loadCache(folder, "S&P 500");
    Matrix<double> nasdaq = loadCache(folder, "NASDAQ"); 
    Matrix<double> wilshire = loadCache(folder, "Wilshire");
    Matrix<double> dowJones = loadCache(folder, "Dow Jones");
    
    Timer t;

    Matrix<double> percentChanges = sp500.column(1);

    
    vector<double> multipliers;
    vector<double> outMultipliers;
    vector<double> pullout_threshs;
    vector<double> backin_threshs;
    vector<int> lookback_days;

    vector<tuple<double, double, double, double, int>> params_list;

    random_device rand_dev {};
    mt19937 generator {rand_dev()};
    uniform_int_distribution<int> multiplierDistr{1, 3};
    uniform_real_distribution<double> outMultiplierDistr{0, 1};
    uniform_real_distribution<double> threshDistr{.8, 1.2};
    uniform_int_distribution<int> lookbackDayDistr{0, 20};

    t.reset();
    // Random Search Parameters
    for (int i = 0; i < 200; i++)
    {
        multipliers.push_back(multiplierDistr(generator));
        outMultipliers.push_back(outMultiplierDistr(generator));
        pullout_threshs.push_back(threshDistr(generator));
        backin_threshs.push_back(threshDistr(generator));
        lookback_days.push_back(lookbackDayDistr(generator));
    }
    cout << t.elapsed() << " seconds to generate params" << endl;


    t.reset();
    FinancialAlgorithm<double, double, double, double, int> alg = algo;
    vector<double> results = testAlgoParams(alg, percentChanges, multipliers, outMultipliers, pullout_threshs, backin_threshs, lookback_days);
    cout << "------------------------------------------" << endl;
    double time = t.elapsed();
    cout << time << " seconds" << endl;
    int max_index = max_element(results.begin(), results.end()) - results.begin();
    cout << "Value: " << fixed << results[max_index] << endl;
    cout << "Params: " << multipliers[max_index] << ", " << outMultipliers[max_index] << ", " << pullout_threshs[max_index] << ", " << backin_threshs[max_index] << ", " << lookback_days[max_index] << endl;


    /*
    t.reset();
    vector<tuple<tuple<double, double, double, double, int>, double>> results = testAlgoParamsParallel<double, double, double, double, int>(percentChanges, algo, params_list);
    auto [bestParams, bestValue] = findBestResult(results);

    cout << "------------------------------------------" << endl;
    double time = t.elapsed();
    cout << time << " seconds" << endl;
    cout << "Best Parameters: " << get<0>(bestParams) << ", " << get<1>(bestParams) << ", " << get<2>(bestParams) << ", " << get<3>(bestParams) << ", " << get<4>(bestParams) << endl;
    cout << "Value: " << bestValue << endl;
    cout << "------------------------------------------" << endl;
    */
}

Matrix<double> algo(Matrix<double> percent_change_data, double multiplier, double out_multiplier, double pullout_thresh, double backin_thresh, int lookback_days)
{    
    if (!percent_change_data.isVector())
        throw std::exception();

    
    Matrix<double> investment_value {percent_change_data.size(), 1};
    investment_value[0] = 1;
    bool in_market = true;
    bool in_market_prev = true;

    for (int i = 1; i < percent_change_data.size(); i++)
    {
        int upper_bound = i;
        int n = lookback_days;
        int lower_bound = max(0, i - n);
        Matrix<double> growth_last_n_days = net_growth(percent_change_data.subVector(upper_bound, lower_bound));

        if (in_market)
            investment_value[i] = investment_value[i - 1] * (1 + multiplier * percent_change_data[i]);
        else
            investment_value[i] = investment_value[i - 1] * (1 + out_multiplier * percent_change_data[i]);;

        if (i > n && growth_last_n_days.min() <= pullout_thresh)
            in_market = false;
        if (i > n && growth_last_n_days.max() >= backin_thresh)
            in_market = true;


        // TODO: Add debug printing here if I actually care


        in_market_prev = in_market;
    }

    return investment_value;
}

Matrix<double> net_growth(Matrix<double> percent_change_data)
{
    if (!percent_change_data.isVector())
        throw std::exception();
    
    Matrix<double> growth {percent_change_data.size(), 1};

    growth[0] = 1 + percent_change_data[0];

    for (int i = 1; i < percent_change_data.size(); i++)
    {
        growth[i] = growth[i - 1] * (1 + percent_change_data[i]);
    }

    return growth;
}