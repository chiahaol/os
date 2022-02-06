#include <ctype.h> 
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "cs402.h"
#include "my402list.h"

#define MAX_LENGTH 1024
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
int interArrivalTime = 0;
int interTokenArrivalTime = 0;
int serviceTime = 0;
char* tsfileName = NULL;
int packetNum = 0;
int curPacketNum = 0;
int tokenNum = 0;
int curTokenNum = 0;
FILE* tsfilePtr = NULL;
struct timeval startTime;
My402List Q1;
My402List Q2;

typedef struct MyThreadParams {
    pthread_mutex_t mutex;
    pthread_cond_t cv;
} MyThreadParams;

typedef struct MyPacket {
    int packetNum;
    struct timeval arriveQ1Time;
    struct timeval leaveQ1Time;
    struct timeval arriveQ2Time;
    struct timeval leaveQ2Time;
    struct timeval arriveServerTime;
    struct timeval leaveServerTime;
    int packetsNeeded;
    int serviceTime;
} MyPacket;



void ParseArgs(int, char**);
void ValidateUniqueArg(char*, double);
double StrToPositiveRealNum(char*);
int StrToPositiveInteger(char*);
void ValidateParam(char*, double, char*);
void OpenTSFile();
void InitParams();
void PrintParams();
void* GeneratingPackets(void* threadParams);
void GetPacketParams(int*);
long long CalculateSleepTime(long long, struct timeval*);
MyPacket* CreatePacket(struct timeval, struct timeval, int, int);
void DropPacket(struct timeval, struct timeval, int);
void GetFormatTimeStamp(char*, struct timeval*);
void* GeneratingTokens(void* threadParams);
void PrintStats();

int main(int argc, char* argv[])
{   
    ParseArgs(argc, argv);
    InitParams();
    PrintParams();

    memset(&Q1, 0, sizeof(My402List));
    My402ListInit(&Q1);
    memset(&Q2, 0, sizeof(My402List));
    My402ListInit(&Q2);

    printf("00000000.000ms: emulation begins\n");
    gettimeofday(&startTime, 0);

    MyThreadParams threadParams = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

    pthread_t packetThread;
    pthread_create(&packetThread, 0, GeneratingPackets, (void*) &threadParams);

    /*pthread_t tokenThread;
    pthread_create(&tokenThread, 0, GenerateTokens, 0);

    pthread_t serverThread1;
    pthread_create(&serverThread1, 0, GenerateTokens, 0);

    pthread_t serverThread2;
    pthread_create(&serverThread2, 0, GenerateTokens, 0);*/


    pthread_join(packetThread, 0);
    //pthread_join(tokenThread, 0);

    struct timeval endTime;
    gettimeofday(&endTime, 0);
    char timestampStr[15];
    GetFormatTimeStamp(timestampStr, &endTime);
    printf("%s: emulation ends\n\n", timestampStr);

    PrintStats();

    return 0;
}

void ParseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Error: argument \"%s\" missing value!!\n", argv[i]);
            exit(-1);
        }

        if (strcmp(argv[i], "-lambda") == 0) {
            ValidateUniqueArg(argv[i], lambda);
            lambda = StrToPositiveRealNum(argv[i+1]);
            ValidateParam(argv[i], lambda, "real number");
        }
        else if (strcmp(argv[i], "-mu") == 0) {
            ValidateUniqueArg(argv[i], mu);
            mu = StrToPositiveRealNum(argv[i+1]);
            ValidateParam(argv[i], mu, "real number");
        }
        else if (strcmp(argv[i], "-r") == 0) {
            ValidateUniqueArg(argv[i], r);
            r = StrToPositiveRealNum(argv[i+1]);
            ValidateParam(argv[i], r, "real number");
        }
        else if (strcmp(argv[i], "-B") == 0) {
            ValidateUniqueArg(argv[i], (double) B);
            B = StrToPositiveInteger(argv[i+1]);
            ValidateParam(argv[i], B, "integer");
        }
        else if (strcmp(argv[i], "-P") == 0) {
            ValidateUniqueArg(argv[i], (double) P);
            P = StrToPositiveInteger(argv[i+1]);
            ValidateParam(argv[i], P, "integer");
        }
        else if (strcmp(argv[i], "-n") == 0) {
            ValidateUniqueArg(argv[i], (double) num);
            num = StrToPositiveInteger(argv[i+1]);
            ValidateParam(argv[i], num, "integer");
        }
        else if (strcmp(argv[i], "-t") == 0) {
            if (tsfileName != NULL) {
                fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", argv[i]);
                exit(-1);
            }
            tsfileName = (char*) malloc(strlen(argv[i+1]) + 1);
            strncpy(tsfileName, argv[i+1], strlen(argv[i+1]) + 1);
            OpenTSFile();
        }
        else {
            fprintf(stderr, "Error: unknown argument \"%s\"!!\n", argv[i]);
            fprintf(stderr, "Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
            exit(-1);
        }
    }
}

void ValidateUniqueArg(char* arg, double var) {
    if (var != 0) {
        fprintf(stderr, "Error: duplicate argument \"%s\"!!\n", arg);
        exit(-1);
    }
}

double StrToPositiveRealNum(char* val) {
    //TODO: remove leading 0s
    char* endPtr;
    double number = strtod(val, &endPtr);
    if (number == 0.0 || *endPtr != '\0') {
        return -1;
    }
    return number;
}

int StrToPositiveInteger(char* val) {
    //TODO: remove leading 0s 
    char* endPtr;
    long long number = strtoll(val, &endPtr, 10);
    if (number == 0LL || number > INT32_MAX || *endPtr != '\0') {
        return -1;
    }
    return (int) number;
}

void ValidateParam(char* arg, double val, char* type) {
    if (val == -1) {
        fprintf(stderr, "Error: value of argument \"%s\" must be a positive %s!!\n", arg, type);
        exit(-1);
    }
}

void OpenTSFile() {
    if (opendir(tsfileName) != NULL) {
        fprintf(stderr, "Error: given file path is a directory (%s)!!\n", tsfileName);
        exit(-1);
    }
    tsfilePtr = fopen(tsfileName, "r");
    if (tsfilePtr == NULL) {
        fprintf(stderr, "Error: cannot open file (%s)!!\n", tsfileName);
        exit(-1);
    }
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
    
    if (tsfilePtr != NULL) {
        char line[MAX_LENGTH + 10];
        if (fgets(line, MAX_LENGTH + 10, tsfilePtr) == NULL) {
            fprintf(stderr, "Error: trace specification file missing number of packets at the first line!!\n");
            exit(-1);
        }
        if (strlen(line) > MAX_LENGTH) {
            fprintf(stderr, "Error in line 1: line length exceeds 1024 characters!!\n");
            exit(-1);
        }
        line[strlen(line) - 1] = '\0';
        num = StrToPositiveInteger(line);
        if (num == -1) {
            fprintf(stderr, "Error in line 1: number of packets must be a positive integer!!\n");
            exit(-1);
        }
    }
    else if (num == 0) {
        num = DEFAULT_NUM;
    }

    interArrivalTime = ((1 / lambda) >= 10) ? 10000 : round(1000 / lambda);
    interTokenArrivalTime = ((1 / r) >= 10) ? 10000 : round(1000 / r);
    serviceTime = ((1 / mu) >= 10) ? 10000 : round(1000 / mu);
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
    /*printf("    inter arrival time = %d\n", interArrivalTime);
    printf("    inter token arrival time = %d\n", interTokenArrivalTime);
    printf("    service time = %d\n", serviceTime);*/
}

void* GeneratingPackets(void* threadParams) {
    MyThreadParams* threadParamsPtr = (MyThreadParams*) threadParams;
    
    int packetParams[3];
    struct timeval prevTime = startTime;
    struct timeval curTime;
    for (int i = 0; i < num; i++) {
        GetPacketParams(packetParams);
        usleep(CalculateSleepTime(packetParams[0] * 1000, &prevTime));
        
        gettimeofday(&curTime, 0);
        if (packetParams[1] > P) {
            DropPacket(prevTime, curTime, packetParams[1]);
        }
        else {
            MyPacket* packet = CreatePacket(prevTime, curTime, packetParams[1], packetParams[2]);
            
            pthread_mutex_lock(&threadParamsPtr->mutex);
            
            My402ListAppend(&Q1, packet);
            
            pthread_mutex_unlock(&threadParamsPtr->mutex);
        }
        prevTime = curTime;
    }
    pthread_exit(0);
}

void GetPacketParams(int* packetParams) {
    if (tsfilePtr == NULL) {
        packetParams[0] = interArrivalTime;
        packetParams[1] = P;
        packetParams[2] = serviceTime;
        return;
    }

    char line[MAX_LENGTH + 10];
    if (fgets(line, MAX_LENGTH + 10, tsfilePtr) == NULL) {
        fprintf(stderr, "Error: the number (%d) of packet data in the trace specification file is less than the specified number (%d) of packets to arrive !!\n", packetNum, num);
        exit(-1);
    }
    
    packetNum++;
    if (strlen(line) > MAX_LENGTH) {
        fprintf(stderr, "Error in tsfile line %d: line length exceeds 1024 characters!!\n", packetNum + 1);
        exit(-1);
    }
    line[strlen(line) - 1] = '\0';

    if (strlen(line) > 0 && (line[0] == ' ' || line[0] == '\t' || line[strlen(line) - 1] == ' ' || line[strlen(line) - 1] == '\t')) {
        fprintf(stderr, "Error in tsfile line %d: contains leading or trailing spaces or tabs!!\n", packetNum + 1);
        exit(-1);
    }

    char* token = strtok(line, " \t");
    int count = 0;
    while (token != NULL && count < 3) {
        packetParams[count] = StrToPositiveInteger(token);
        if (packetParams[count] == -1) {
            fprintf(stderr, "Error in tsfile line %d: packet data must be a positive integer (%s)!!\n", packetNum + 1, token);
            exit(-1);
        }
        token = strtok(NULL, " \t");
        count++;
    }

    if (token != NULL || count < 3) {
        fprintf(stderr, "Error in tsfile line %d: number of fields must be 3!!\n", packetNum + 1);
        exit(-1);
    }
}

long long CalculateSleepTime(long long target, struct timeval* prevTime) {
    struct timeval curTime;
    gettimeofday(&curTime, 0);
    long long timeDiff = (curTime.tv_sec - prevTime->tv_sec) * 1000000 + (curTime.tv_usec - prevTime->tv_usec);
    return (timeDiff < target) ? (target - timeDiff) : 0;
}

MyPacket* CreatePacket(struct timeval prevTime, struct timeval curTime, int packetsNeeded, int serviceTime) {
    char timestampStr[15];
    GetFormatTimeStamp(timestampStr, &curTime);
    long long curInterArrivalTime = (curTime.tv_sec - prevTime.tv_sec) * 1000000 + (curTime.tv_usec - prevTime.tv_usec);

    MyPacket* packet = (MyPacket*) malloc(sizeof(MyPacket));
    packet->packetNum = packetNum;
    packet->arriveQ1Time = curTime;
    packet->packetsNeeded = packetsNeeded;
    packet->serviceTime = serviceTime;
    printf("%s: p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n", timestampStr, packetNum, packetsNeeded, curInterArrivalTime / 1000.0);
    return packet;
}

void DropPacket(struct timeval prevTime, struct timeval curTime, int packetsNeeded) {
    char timestampStr[15];
    GetFormatTimeStamp(timestampStr, &curTime);
    long long curInterArrivalTime = (curTime.tv_sec - prevTime.tv_sec) * 1000000 + (curTime.tv_usec - prevTime.tv_usec);
    printf("%s: p%d arrives, needs %d tokens, inter-arrival time = %.3fms, dropped\n", timestampStr, packetNum, packetsNeeded, curInterArrivalTime / 1000.0);
}

void GetFormatTimeStamp(char* timestampStr, struct timeval* curTime) {
    memset(timestampStr, '0', 11);
    long long timeDiff = (curTime->tv_sec - startTime.tv_sec) * 1000000 + (curTime->tv_usec - startTime.tv_usec);
    char buff[10];
    snprintf(buff, 10, "%lld", timeDiff % 1000);
    strncpy(timestampStr + 12 - strlen(buff), buff, strlen(buff));
    snprintf(buff, 10, "%lld", timeDiff / 1000);
    strncpy(timestampStr + 8 - strlen(buff), buff, strlen(buff));
    timestampStr[8] = '.';
    timestampStr[12] = 'm';
    timestampStr[13] = 's';
    timestampStr[14] = '\0';
}

void* GeneratingTokens(void* threadParams) {
    pthread_exit(0);
}

void PrintStats() {
    return;
}
