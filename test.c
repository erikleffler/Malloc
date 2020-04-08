#include "malloc.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{

	printf("===============TEST 1 WILL PROCEED=================\n");

	/* Test 1, malloc och sen realloc*/

	char *s1, *s2;
	s1 =  malloc1(5);
	printf("RETURNED POINTER FROM MALLOC1: %p \n", s1);
	strcpy(s1, "heja");
	printf("DATA POINTED TO BY S1: %s \n", s1);
	fflush(stdout);
	s2 = realloc1(s1, 20);
	printf("Realloc pointer: %p \n", s2);
	char string[20] = "1234567890123456789\0";
	printf("Did realloc\n");
	fflush(stdout);
	strcpy(s2, string);
	printf("%s \n", s2);
	free1(s2);
	printf("DATA POINTED TO BY S1 AFTER REALLOC WITHOUT OVERWRITE: %s \n", s1);

	/* Test 2, många malloc följt av free följt av stor malloc */

	printf("===============TEST 2 WILL PROCEED=================\n");

	char *text[5];
	for(int i = 0; i < 5; i++)
	{
		text[i] = malloc1(5);
	}
	for(int i = 0; i < 5; i++)
	{
		printf("%s\n", text[i]);
	}
	for(int i = 0; i < 5; i++)
	{
		free1(text[i]);
	}
}
