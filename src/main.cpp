#include <iostream>
#include <thread>
#include <future>
#include <vector>
#include <unordered_map>

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

Matrix<double> algo(Matrix<double> percent_change_data, tuple<double, double, double, int> parameters);
Matrix<double> net_growth(Matrix<double> percent_change_data);

vector<int> getSplitIndexes(int length, int groups);
vector<double> getParamRange(double lowVal, double highVal, int num);

int main()
{
    // Load data
    Matrix<double> daily_values = Matrix<double>::fromFile("../data/Daily Values 1980-Present.csv");    
    std::cout << daily_values.shape() << std::endl;

    if (!daily_values.isVector())
        throw std::exception();

    Matrix<double> percent_change {daily_values.size(), 1};
    // Calculate percent change values for each day
    for (int i = 1; i < daily_values.size(); i++)
    {
        percent_change[i] = ((daily_values[i] / daily_values[i-1]) - 1) * 100;
    }

    vector<double> multipliers = getParamRange(1, 3, 10);
    vector<double> pullout_thresholds = getParamRange(.8, 1.2, 10);
    vector<double> backin_thresholds = getParamRange(.8, 1.2, 10);

    vector<tuple<double, double, double, int>> params_list;

    for (double m : multipliers)
    {
        for (double p : pullout_thresholds)
        {
            for (double b : backin_thresholds)
            {
                params_list.push_back({m, p, b, 10});
            }
        }
    }

    Timer t;

    vector<tuple<tuple<double, double, double, int>, double>> results = testAlgoParamsParallel<double, double, double, int>(percent_change, algo, params_list);
    auto [bestParams, bestValue] = findBestResult(results);

    cout << "------------------------------------------" << endl; 
    double time = t.elapsed();
    cout << time << " seconds" << endl;
    cout << "Best Parameters: " << get<0>(bestParams) << ", " << get<1>(bestParams) << ", " << get<2>(bestParams) << endl;
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

Matrix<double> algo(Matrix<double> percent_change_data, tuple<double, double, double, int> parameters)
{
    double multiplier = get<0>(parameters);
    double pullout_thresh = get<1>(parameters);
    double backin_thresh = get<2>(parameters);
    int lookback_days = get<3>(parameters);
    
    
    
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
            investment_value[i] = investment_value[i - 1] * (1 + in_market * multiplier * percent_change_data[i] / 100);
        else
            investment_value[i] = investment_value[i - 1];

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

    growth[0] = 1 + percent_change_data[0] / 100;

    for (int i = 1; i < percent_change_data.size(); i++)
    {
        growth[i] = growth[i - 1] * (1 + percent_change_data[i] / 100);
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