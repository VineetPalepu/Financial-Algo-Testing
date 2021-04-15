#include <thread>
#include <future>
#include <vector>
#include <string>
#include <filesystem>

#include "Matrix.h"

using namespace std;
using namespace MatrixLibrary;

template<typename... HyperParameters>
using FinancialAlgorithm = function<Matrix<double>(Matrix<double>, HyperParameters...)>;

template<std::size_t N>
struct num { static const constexpr auto value = N; };

template<class Function, size_t... Is>
void for_(Function f, index_sequence<Is...>){ (f(num<Is>{}), ...); }

template <std::size_t N, typename F>
void for_(F func) { for_(func, std::make_index_sequence<N>()); }

Matrix<double> loadCache(string filepath, string filename)
{
    if (filesystem::exists(filepath + filename + ".dat"))
    {
        cout << "Loading " << filepath << filename << ".dat" << endl;
        return Matrix<double>::load(filepath + filename + ".dat");
    }
    else
    {
        cout << filepath << filename << ".dat" << " does not exist. Loading csv instead. This may take a while." << endl; 
        cout << "Loading " << filepath << filename << ".csv" << endl; 
        Matrix<double> mat = Matrix<double>::fromFile(filepath + filename + ".csv");
        cout << "Saving " << filepath << filename << ".dat" << endl; 
        mat.save(filepath + filename + ".dat");
        return mat;
    }
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
void testAlgoParamsParallel(FinancialAlgorithm<HyperParameters...> algorithm, Matrix<double> percent_changes_data, vector<HyperParameters>... params_list)
//vector<tuple<tuple<HyperParameters ...>, double>> testAlgoParamsParallel(Matrix<double> percent_changes_data, FinancialAlgorithm<HyperParameters ...> alg, vector<tuple<HyperParameters ...>> paramsList)
{
    int cores = thread::hardware_concurrency();
    tuple<vector<HyperParameters>...> params_tuple = make_tuple(params_list...);

    vector<future<vector<double>>> futures;

    int prevIndex = 0;
    for (int index : getSplitIndexes(paramsList.size(), cores))
    {
        //vector<HyperParameters ...>> params (paramsList.begin() + prevIndex, paramsList.begin() + index);

        futures.push_back(async(testAlgoParams<HyperParameters ...>, percent_changes_data, alg, params));

        prevIndex = index;
    }
    /*
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
    */
}

template<typename... HyperParameters>
vector<double> testAlgoParams(FinancialAlgorithm<HyperParameters ...> algorithm, Matrix<double> percent_changes_data, vector<HyperParameters>... params_list)
{
    tuple<vector<HyperParameters>...> params_tuple = make_tuple(params_list...);

    constexpr int num_args = tuple_size<decltype(params_tuple)>::value;
    int num_combos = (num_args == 0 ? 0 : get<0>(params_tuple).size());
    vector<double> values;
    for (int i = 0; i < num_combos; i++)
    {
        tuple<HyperParameters...> arg_tup;
        for_<num_args>([&] (auto j)
        {
            get<j.value>(arg_tup) = get<j.value>(params_tuple)[i];
        });
        Matrix<double> result = apply(algorithm, tuple_cat(make_tuple(percent_changes_data), arg_tup));
        values.push_back(result[-1]);
    }

    return values;
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