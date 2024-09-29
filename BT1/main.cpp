#include <iostream>
#include <iomanip>

#define CONG 0x0001
#define TRU 0x0002
#define NHAN 0x0004
#define CHIA 0x0008
#define MAX 0x0010
#define MIN 0x0020

void calculator(unsigned int MathType, float num1, float num2)
{
    if (MathType & CONG)
    {
        std::cout << "addition is : " << num1 + num2 << std::fixed << std::setprecision(2) << std::endl;
    }

    if (MathType & TRU)
    {
        std::cout << "subtraction is : " << num1 - num2 << std::fixed << std::setprecision(2) << std::endl;
    }

    if (MathType & NHAN)
    {
        std::cout << "multiplication is : " << num1 * num2 << std::fixed << std::setprecision(2) << std::endl;
    }

    if (MathType & CHIA)
    {
        if (num2 != 0)
        {
            std::cout << "divison is : " << num1 / num2 << std::fixed << std::setprecision(2) << std::endl;
        }

        else
        {
            std::cout << "ERROR devide by 0" << std::endl;
        }
    }
    if (MathType & MAX)
    {
        if (num2 == num1)
        {
            std::cout << "num1=num2" << std::endl;
        }

        else
        {
            std::cout << "largest: " << (num1 > num2 ? num1 : num2) << std::endl;
        }
    }

    if (MathType & MIN)
    {
        if (num2 == num1)
        {
            std::cout << "num1=num2" << std::endl;
        }

        else
        {
            std::cout << "smallest: " << (num1 < num2 ? num1 : num2) << std::endl;
        }
    }
}

int main(int argc, char* argv[])
{
    float a=9, b=9;
    calculator(CONG|TRU|NHAN|CHIA|MAX|MIN,a,b);

    return 0;
}