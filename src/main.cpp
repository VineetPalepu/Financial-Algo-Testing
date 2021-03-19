#include <iostream>
#include <thread>

#include "Matrix.h"
#include "Timer.h"

using namespace std;
using namespace MatrixLibrary;

Matrix<double> algo(Matrix<double> percent_change_data, double multiplier, double pullout_thresh, double backin_thresh);
Matrix<double> net_growth(Matrix<double> percent_change_data);

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


    Timer t;
    auto values = algo(percent_change, 3, .9, 1.1, 10);
    cout << t.elapsed() * 1000 << " milliseconds" << endl;
    cout << "Value: " << values[-1] << endl;
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