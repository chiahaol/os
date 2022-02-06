#include <ctype.h> 
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "cs402.h"

#include "my402list.h"

#define DEFAULT_LAMBDA 1
#define DEFAULT_MU 0.35
#define DEFAULT_R 1.5
#define DEFAULT_B 10
#define DEFAULT_P 3
#define DEFAULT_NUM 20

double lambda = 0;
double mu = 0;
double r = 0;
int B = 0;
int P = 0;
int num = 0;
char* tsfileName = NULL;

void ParseArgs(int, char**);
double ValidatePositiveRealNum(char*, char*);
int ValidatePositiveInteger(char*, char*);
void InitParams();
void PrintParams();

int main(int argc, char* argv[])
{   
    ParseArgs(argc, argv);
    InitParams();
    PrintParams();

    
    return 0;
}

void ParseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Error: argument %s missing value!!\n", argv[i]);
            exit(-1);
        }

        if (strcmp(argv[i], "-lambda") == 0) {
            if (lambda != 0) {
                fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", argv[i]);
                exit(-1);
            }
            lambda = ValidatePositiveRealNum(argv[i], argv[i+1]);
        }
        else if (strcmp(argv[i], "-mu") == 0) {
            if (mu != 0) {
                fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", argv[i]);
                exit(-1);
            }
            mu = ValidatePositiveRealNum(argv[i], argv[i+1]);
        }
        else if (strcmp(argv[i], "-r") == 0) {
            if (r != 0) {
                fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", argv[i]);
                exit(-1);
            }
            r = ValidatePositiveRealNum(argv[i], argv[i+1]);
        }
        else if (strcmp(argv[i], "-B") == 0) {
            if (B != 0) {
                fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", argv[i]);
                exit(-1);
            }
            B = ValidatePositiveInteger(argv[i], argv[i+1]);
        }
        else if (strcmp(argv[i], "-P") == 0) {
            if (P != 0) {
                fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", argv[i]);
                exit(-1);
            }
            P = ValidatePositiveInteger(argv[i], argv[i+1]);
        }
        else if (strcmp(argv[i], "-n") == 0) {
            if (num != 0) {
                fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", argv[i]);
                exit(-1);
            }
            num = ValidatePositiveInteger(argv[i], argv[i+1]);
        }
        else if (strcmp(argv[i], "-t") == 0) {
            if (tsfileName != NULL) {
                fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", argv[i]);
                exit(-1);
            }
            tsfileName = (char*) malloc(strlen(argv[i+1]) + 1);
            strncpy(tsfileName, argv[i+1], strlen(argv[i+1]) + 1);
        }
        else {
            fprintf(stderr, "Error: unknown argument \"%s\"!!\n", argv[i]);
            fprintf(stderr, "Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
            exit(-1);
        }

    }
}

double ValidatePositiveRealNum(char* arg, char* val) {
    char* endPtr;
    double number = strtod(val, &endPtr);
    if (number == 0.0 || *endPtr != '\0') {
        fprintf(stderr, "Error: argument \"%s\" must be a positive real number!!\n", arg);
        exit(-1);
    }
    return number;
}

int ValidatePositiveInteger(char* arg, char* val) {
    char* endPtr;
    long long number = strtoll(val, &endPtr, 10);
    if (number == 0LL || *endPtr != '\0') {
        fprintf(stderr, "Error: argument \"%s\" must be a positive integer!!\n", arg);
        exit(-1);
    }
    if (number > INT32_MAX) {
        fprintf(stderr, "Error: argument \"%s\" exceeds maximum value (%d)!!\n", arg, INT32_MAX);
        exit(-1);
    }
    return (int) number;
}

void InitParams() {
    if (lambda == 0) {
        lambda = DEFAULT_LAMBDA;
    }
    if (mu == 0) {
        mu = DEFAULT_MU;
    }
    if (r == 0) {
        r = DEFAULT_R;
    }
    if (B == 0) {
        B = DEFAULT_B;
    }
    if (P == 0) {
        P = DEFAULT_P;
    }
    if (num == 0) {
        num = DEFAULT_NUM;
    }
}

void PrintParams() {
    printf("Emulation Parameters:\n");
    printf("    number to arrive = %d\n", num);
    if (tsfileName == NULL) {
        printf("    lambda = %.6g\n", lambda);
    }
    if (tsfileName == NULL) {
        printf("    mu = %.6g\n", mu);
    }
    printf("    r = %.6g\n", r);
    printf("    B = %d\n", B);
    if (tsfileName == NULL) {
        printf("    P = %d\n", P);
    }
    if (tsfileName != NULL) {
        printf("    tsfile = %s\n\n", tsfileName);
    }
}
