// BirdeeMainTest.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>

extern "C" int link__test_0addprint(int a,int b);
extern "C" void link__test_0_1main();

extern "C" int link__test_0a;

int main()
{
	link__test_0_1main();
	printf("link_test.addprint(%d,%d)=%d\n", 23, 32, link__test_0addprint(23, 32));
	printf("link_test.a=%d\n", link__test_0a);
	//birdeemain();
    return 0;
}

