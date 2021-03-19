#include <iostream>
#include <thread>
#include <future>
#include <vector>
#include <unordered_map>

#include "Matrix.h"
#include "Timer.h"

using namespace std;
using namespace MatrixLibrary;

vector<int> getSplitIndexes(int length, int groups);
vector<double> getParamRange(double lowVal, double highVal, int num);
Matrix<double> algo(Matrix<double> percent_change_data, double multiplier, double pullout_thresh, double backin_thresh, int lookback_days);
Matrix<double> net_growth(Matrix<double> percent_change_data);

struct hyperparameters
{
    double multiplier;
    double pullout_thresh;
    double backin_thresh;

    bool operator==(const hyperparameters& o) const
    {
        return o.multiplier == multiplier && o.pullout_thresh == pullout_thresh && o.backin_thresh == backin_thresh;
    }
};

template <>
struct hash<hyperparameters>
{
    size_t operator()(const hyperparameters& params) const
    {
        return hash<double>()(params.multiplier) ^ hash<double>()(params.pullout_thresh) ^ hash<double>()(params.backin_thresh);
    }
};

using FinancialAlgorithm = Matrix<double> (*)(Matrix<double>, double, double, double, int);

vector<tuple<hyperparameters, double>> findOptimal(FinancialAlgorithm alg, vector<hyperparameters> paramsList, Matrix<double> percent_changes_data);

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

    vector<double> multipliers = getParamRange(1, 3, 50);
    vector<double> pullout_thresholds = getParamRange(.8, 1.2, 50);
    vector<double> backin_thresholds = getParamRange(.8, 1.2, 50);

    vector<hyperparameters> params_list;

    for (double m : multipliers)
    {
        for (double p : pullout_thresholds)
        {
            for (double b : backin_thresholds)
            {
                params_list.push_back({m, p, b});
            }
        }
    }

    Timer t;

    int cores = thread::hardware_concurrency();
    int prevIndex = 0;


    vector<future<vector<tuple<hyperparameters, double>>>> futures;

    for (int index : getSplitIndexes(params_list.size(), cores))
    {
        vector<hyperparameters> params (params_list.begin() + prevIndex, params_list.begin() + index);

        futures.push_back(async(findOptimal, algo, params, percent_change));

        prevIndex = index;
    }

    vector<tuple<hyperparameters, double>> results;

    for (uint8_t i = 0; i < futures.size(); i++)
    {
        vector<tuple<hyperparameters, double>> res = futures[i].get();
        
        results.insert(results.end(), res.begin(), res.end());
    }



    hyperparameters bestParams;
    double bestValue = 0;
    for (auto result : results)
    {
        hyperparameters params = get<0>(result);
        double value = get<1>(result);
        
        //cout << "(" << params.multiplier << ", "  << params.pullout_thresh << ", " << params.backin_thresh << ")" << ": " << get<1>(result) << endl;

        if (get<1>(result) > bestValue)
        {
            bestValue = get<1>(result);
            bestParams = get<0>(result);
        }
    }

    cout << "------------------------------------------" << endl; 
    double time = t.elapsed();
    cout << cores << " Threads" << endl;
    cout << time << " seconds" << endl;
    cout << "Best Parameters: " << bestParams.multiplier << ", " << bestParams.pullout_thresh << ", " << bestParams.backin_thresh << endl;
    cout << "Value: " << bestValue << endl;
    cout << "------------------------------------------" << endl;


    cout << "Parameters: 3, .9, 1.1" << endl;
    cout << "Value: " << algo(percent_change, 3, .9, 1.1, 7)[-1] << endl;

    cout << "Param Diff: " << abs(bestParams.multiplier - 3) << ", " << abs(bestParams.pullout_thresh - .9) << ", " << abs(bestParams.backin_thresh - 1.1) << endl;
    cout << "Value Ratio: " << bestValue / algo(percent_change, 3, .9, 1.1, 7)[-1] << endl;
}

vector<tuple<hyperparameters, double>> findOptimal(FinancialAlgorithm alg, vector<hyperparameters> paramsList, Matrix<double> percent_changes_data)
{
    vector<tuple<hyperparameters, double>> results;
    for (auto params : paramsList)
    {
        double finalVal = alg(percent_changes_data, params.multiplier, params.pullout_thresh, params.backin_thresh, 7)[-1];
        results.push_back({params, finalVal});
    }

    return results;
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

Matrix<double> algo(Matrix<double> percent_change_data, double multiplier, double pullout_thresh, double backin_thresh, int lookback_days)
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