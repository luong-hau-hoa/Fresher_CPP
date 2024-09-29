#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;

cpp_int fibonacci_from_100_down(int N)
{
    if (N == 0)
    {
        return 0;
    }
    if (N == 1)
    {
        return 1;
    }

    cpp_int fibonacci_sub1 = 1, fibonacci_sub2 = 0, fibonacci;
    for (int i = 2; i <= N; i++)
    {
        fibonacci = fibonacci_sub1+fibonacci_sub2;
        fibonacci_sub2 = fibonacci_sub1;
        fibonacci_sub1 =fibonacci;
    }
    return fibonacci;
}

int main()
{
    int test_values[] = {0,1,10,11,20,30,40,50,60,70,80,90,100};
    int number_of_elements =sizeof(test_values)/sizeof(test_values[0]);
    for(int i=0; i<number_of_elements; i++)
    {
        int N =test_values[i];
        cpp_int result = fibonacci_from_100_down(N);
        std::cout << "Fibinacci number " << N << " is:" << result << std::endl;
    };
    return 0;
}