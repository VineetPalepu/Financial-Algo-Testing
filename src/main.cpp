#include <iostream>
#include <thread>
#include <future>
#include <vector>
#include <unordered_map>
#include <string>
#include <random>

#include "Matrix.h"
#include "Timer.h"

using namespace std;
using namespace MatrixLibrary;

template<typename... HyperParameters>
using FinancialAlgorithm = function<Matrix<double>(Matrix<double>, tuple<HyperParameters...>)>;

template<typename... HyperParameters>
tuple<tuple<HyperParameters ...>, double> findBestResult(vector<tuple<tuple<HyperParameters ...>, double>> results);

template<typename... HyperParameters>
vector<tuple<tuple<HyperParameters ...>, double>> testAlgoParamsParallel(Matrix<double> percent_changes_data, FinancialAlgorithm<HyperParameters ...> alg, vector<tuple<HyperParameters ...>> paramsList);

template<typename... HyperParameters>
vector<tuple<tuple<HyperParameters ...>, double>> testAlgoParams(Matrix<double> percent_changes_data, FinancialAlgorithm<HyperParameters ...> alg, vector<tuple<HyperParameters ...>> paramsList);

Matrix<double> algo(Matrix<double> percent_change_data, tuple<double, double, double, double, int> parameters);
Matrix<double> net_growth(Matrix<double> percent_change_data);

tuple<Matrix<double>, Matrix<double>> getDataFromCSV(string filepath);

vector<int> getSplitIndexes(int length, int groups);
vector<double> getParamRange(double lowVal, double highVal, int num);

int main()
{
    Matrix<double> a {2,3};
    a.randInit();
    a.print();
    a.save("a.mat");
    a.print();
    Matrix<double> b = Matrix<double>::load("a.mat");
    b.print();


    
    Matrix<double> sp500 = Matrix<double>::fromFile("../data/S&P 500.csv");
    //Matrix<double> nasdaq = Matrix<double>::fromFile("../data/NASDAQ.csv");
    //Matrix<double> wilshire = Matrix<double>::fromFile("../data/Wilshire.csv");
    //Matrix<double> dowJones = Matrix<double>::fromFile("../data/Dow Jones.csv");

    Matrix<double> percentChanges = sp500.column(1);

    //cout << algo(percentChanges, {})
    
    vector<tuple<double, double, double, double, int>> params_list;

    random_device rand_dev {};
    mt19937 generator {rand_dev()};
    uniform_int_distribution<int> multiplierDistr{1, 3};
    uniform_real_distribution<double> outMultiplierDistr{0, 1};
    uniform_real_distribution<double> threshDistr{.8, 1.2};
    uniform_int_distribution<int> lookbackDayDistr{0, 20};

    Timer t;
    // Random Search Parameters
    for (int i = 0; i < 20000; i++)
    {
        params_list.push_back({multiplierDistr(generator), outMultiplierDistr(generator), threshDistr(generator), threshDistr(generator), lookbackDayDistr(generator)});
    }
    cout << t.elapsed() << " seconds to generate params" << endl;
    
    t.reset();

    vector<tuple<tuple<double, double, double, double, int>, double>> results = testAlgoParamsParallel<double, double, double, double, int>(percentChanges, algo, params_list);
    auto [bestParams, bestValue] = findBestResult(results);

    cout << "------------------------------------------" << endl;
    double time = t.elapsed();
    cout << time << " seconds" << endl;
    cout << "Best Parameters: " << get<0>(bestParams) << ", " << get<1>(bestParams) << ", " << get<2>(bestParams) << ", " << get<3>(bestParams) << ", " << get<4>(bestParams) << endl;
    cout << "Value: " << bestValue << endl;
    cout << "------------------------------------------" << endl;
}

template<typename... HyperParameters>
tuple<tuple<HyperParameters ...>, double> findBestResult(vector<tuple<tuple<HyperParameters ...>, double>> results)
{
    tuple<HyperParameters ...> bestParams;
    double bestValue = 0;
    for (auto pair : results)
    {
        double value = get<1>(pair);
        if (value > bestValue)
        {
            bestValue = value;
            bestParams = get<0>(pair);
        }
    }
    return {bestParams, bestValue};
}

template<typename... HyperParameters>
vector<tuple<tuple<HyperParameters ...>, double>> testAlgoParamsParallel(Matrix<double> percent_changes_data, FinancialAlgorithm<HyperParameters ...> alg, vector<tuple<HyperParameters ...>> paramsList)
{
    int cores = thread::hardware_concurrency();

    vector<future<vector<tuple<tuple<HyperParameters ...>, double>>>> futures;

    int prevIndex = 0;
    for (int index : getSplitIndexes(paramsList.size(), cores))
    {
        vector<tuple<HyperParameters ...>> params (paramsList.begin() + prevIndex, paramsList.begin() + index);

        futures.push_back(async(testAlgoParams<HyperParameters ...>, percent_changes_data, alg, params));

        prevIndex = index;
    }

    vector<tuple<tuple<HyperParameters ...>, double>> results;
    for (int i = 0; i < cores; i++)
    {
        vector<tuple<tuple<HyperParameters ...>, double>> result = futures[i].get();
        results.insert(results.end(), result.begin(), result.end());
    }

    return results;
}

template<typename... HyperParameters>
vector<tuple<tuple<HyperParameters ...>, double>> testAlgoParams(Matrix<double> percent_changes_data, FinancialAlgorithm<HyperParameters ...> alg, vector<tuple<HyperParameters ...>> paramsList)
{
    vector<tuple<tuple<HyperParameters...>, double>> results;
    for (auto params : paramsList)
    {
        double finalVal = alg(percent_changes_data, params)[-1];
        results.push_back({params, finalVal});
    }

    return results;
}

//Matrix<double> 

Matrix<double> algo(Matrix<double> percent_change_data, tuple<double, double, double, double, int> parameters)
{
    double multiplier = get<0>(parameters);
    double outMultiplier = get<1>(parameters);
    double pullout_thresh = get<2>(parameters);
    double backin_thresh = get<3>(parameters);
    int lookback_days = get<4>(parameters);
    
    
    
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
            investment_value[i] = investment_value[i - 1] * (1 + outMultiplier * percent_change_data[i]);;

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

vector<int> getSplitIndexes(int length, int groups)
{
    vector<int> groupSizes;

    int initialSize = length / groups;
    for (int i = 0; i < groups; i++)
    {
        groupSizes.push_back(initialSize);
    }

    int remainder = length % groups;
    for (int i = 0; i < remainder; i++)
    {
        groupSizes[i] += 1;
    }

    vector<int> splitIndexes;
    int curIndex = 0;
    for (int size : groupSizes)
    {
        curIndex += size;
        splitIndexes.push_back(curIndex);
    }

    return splitIndexes;

}

vector<double> getParamRange(double lowVal, double highVal, int num)
{
    vector<double> range;

    double delta = (highVal - lowVal) / (num - 1);
    for (int i = 0; i < num - 1; i++)
    {
        range.push_back(lowVal + delta * i);
    }
    range.push_back(highVal);

    return range;
}