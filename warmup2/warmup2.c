#include <ctype.h> 
#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
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

typedef struct MyPacket {
    int packetNum;
    struct timeval arriveSystemTime;
    struct timeval arriveQ1Time;
    struct timeval leaveQ1Time;
    struct timeval arriveQ2Time;
    struct timeval leaveQ2Time;
    struct timeval arriveServerTime;
    struct timeval leaveServerTime;
    int packetsNeeded;
    int serviceTime;
} MyPacket;

typedef struct SystemStats {
    double interArrivalTimeRunAvg;
    double serviceTimeRunAvg;
    double timeInQ1RunAvg;
    double timeInQ2RunAvg;
    double timeInS1RunAvg;
    double timeInS2RunAvg;
    double systemTimeRunAvg;
    double systemTimeSqrRunAvg;
    int totalPacketNum;
    int completedPacket;
    int droppedPacket;
    int totalTokenNum;
    int bucketTokenNum;
    int droppedToken;
    int departFromS1;
    int departFromS2;
} SystemStats;

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
FILE* tsfilePtr = NULL;
struct timeval startTime;
struct timeval endTime;
int shouldTerminate = 0;
My402List Q1;
My402List Q2;
SystemStats systemStats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_t signalThread;
pthread_t packetThread;
pthread_t tokenThread;
pthread_t serverThread1;
pthread_t serverThread2;

void ParseArgs(int, char**);
void ValidateUniqueArg(char*, double);
double StrToPositiveRealNum(char*);
int StrToPositiveInteger(char*);
void ValidateParam(char*, double, char*);
void OpenTSFile();
void InitParams();
void PrintParams();
void* GeneratingPackets(void*);
void GetPacketParams(int*);
void CheckTSFileEnds();
long long timeDiffMicroSec(struct timeval*, struct timeval*);
void SleepAdjustedAmountOfTime(long long, struct timeval*);
void MySleep(long long);
MyPacket* CreatePacket(struct timeval*, struct timeval*, int, int);
void SendPacketToQ1(MyPacket*);
void GenerateTraceTimestamp(char*, struct timeval*);
void* GeneratingTokens(void*);
int InsertToken(struct timeval*);
void SendPacketFromQ1ToQ2();
int NoMorePacketsToCome();
void* Server(void*);
void GetPacketFromQ2(MyPacket*, int);
void TransmitPacket(MyPacket*, int);
void* HandlingSignal(void*);
void RemoveAllPackets();
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

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, 0);

    pthread_create(&signalThread, 0, HandlingSignal, (void*) &set);
    pthread_create(&packetThread, 0, GeneratingPackets, 0);
    pthread_create(&tokenThread, 0, GeneratingTokens, 0);
    pthread_create(&serverThread1, 0, Server, (void*) 1);
    pthread_create(&serverThread2, 0, Server, (void*) 2);

    pthread_join(packetThread, 0);
    pthread_join(tokenThread, 0);
    pthread_join(serverThread1, 0);
    pthread_join(serverThread2, 0);

    gettimeofday(&endTime, 0);
    char timestampStr[15];
    GenerateTraceTimestamp(timestampStr, &endTime);
    printf("%s: emulation ends\n\n", timestampStr);

    PrintStats();

    if (tsfilePtr != NULL) {
        fclose(tsfilePtr);
    }

    return 0;
}

void ParseArgs(int argc, char** argv) {
    int traceDrivenMode = 0;
    for (int i = 1; i < argc; i ++) {
        if (strcmp(argv[i], "-t") == 0) {
            traceDrivenMode = 1;
            break;
        }
    }

    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Error: argument \"%s\" missing value!!\n", argv[i]);
            fprintf(stderr, "Usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
            exit(-1);
        }

        if (strcmp(argv[i], "-lambda") == 0 ) {
            if (traceDrivenMode) continue;
            ValidateUniqueArg(argv[i], lambda);
            lambda = StrToPositiveRealNum(argv[i+1]);
            ValidateParam(argv[i], lambda, "real number");
        }
        else if (strcmp(argv[i], "-mu") == 0) {
            if (traceDrivenMode) continue;
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
            if (traceDrivenMode) continue;
            ValidateUniqueArg(argv[i], (double) P);
            P = StrToPositiveInteger(argv[i+1]);
            ValidateParam(argv[i], P, "integer");
        }
        else if (strcmp(argv[i], "-n") == 0) {
            if (traceDrivenMode) continue;
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
    if (number == 0LL || number > INT_MAX || *endPtr != '\0') {
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
        printf("    tsfile = %s\n", tsfileName);
    }
    printf("\n");
}

void* GeneratingPackets(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

    int packetParams[3];
    struct timeval prevTime = startTime;
    struct timeval curTime;
    for (int i = 0; i < num; i++) {
        GetPacketParams(packetParams);

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
        SleepAdjustedAmountOfTime(packetParams[0] * 1000, &prevTime);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

        pthread_mutex_lock(&mutex);
        
        if (shouldTerminate == 1) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        gettimeofday(&curTime, 0);
        MyPacket* packet = CreatePacket(&prevTime, &curTime, packetParams[1], packetParams[2]);
        if (packet != NULL) {
            SendPacketToQ1(packet);
            if (Q1.num_members == 1 && systemStats.bucketTokenNum >= packet->packetsNeeded) {
                SendPacketFromQ1ToQ2(packet);
                pthread_cond_broadcast(&cv);
            }  
        }

        if (systemStats.totalPacketNum == num) {
            if (tsfilePtr != NULL) {
                CheckTSFileEnds();
            }
            if (My402ListEmpty(&Q1)) {
                pthread_cancel(tokenThread);
                pthread_cond_broadcast(&cv);
            }
        }

        pthread_mutex_unlock(&mutex);

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
        fprintf(stderr, "Error: the number (%d) of packet data in the trace specification file is less than the specified number (%d) of packets to arrive !!\n", systemStats.totalPacketNum, num);
        exit(-1);
    }
    
    if (strlen(line) > MAX_LENGTH) {
        fprintf(stderr, "Error in tsfile line %d: line length exceeds 1024 characters!!\n", systemStats.totalPacketNum + 2);
        exit(-1);
    }
    line[strlen(line) - 1] = '\0';

    if (strlen(line) > 0 && (line[0] == ' ' || line[0] == '\t' || line[strlen(line) - 1] == ' ' || line[strlen(line) - 1] == '\t')) {
        fprintf(stderr, "Error in tsfile line %d: contains leading or trailing spaces or tabs!!\n", systemStats.totalPacketNum + 2);
        exit(-1);
    }

    char* token = strtok(line, " \t");
    int count = 0;
    while (token != NULL && count < 3) {
        packetParams[count] = StrToPositiveInteger(token);
        if (packetParams[count] == -1) {
            fprintf(stderr, "Error in tsfile line %d: packet data must be a positive integer (%s)!!\n", systemStats.totalPacketNum + 2, token);
            exit(-1);
        }
        token = strtok(NULL, " \t");
        count++;
    }

    if (token != NULL || count < 3) {
        fprintf(stderr, "Error in tsfile line %d: number of fields must be 3!!\n", systemStats.totalPacketNum + 2);
        exit(-1);
    }
}

void CheckTSFileEnds() {
    char line[MAX_LENGTH + 10];
    if (fgets(line, MAX_LENGTH + 10, tsfilePtr) != NULL) {
        fprintf(stderr, "Error: the number of packet data in the trace specification file exceeds the specified number (%d) of packets to arrive !!\n", num);
        exit(-1);
    }
}

long long timeDiffMicroSec(struct timeval* curTime, struct timeval* prevTime) {
    return (curTime->tv_sec - prevTime->tv_sec) * 1000000 + (curTime->tv_usec - prevTime->tv_usec);
}

void SleepAdjustedAmountOfTime(long long targetSleepTime, struct timeval* prevTime) {
    struct timeval curTime;
    gettimeofday(&curTime, 0);
    long long timeDiff = timeDiffMicroSec(&curTime, prevTime);
    MySleep(targetSleepTime - timeDiff);
}

void MySleep(long long targetSleepTime) {
    long long sec = targetSleepTime / 1000000;
    long long usec = targetSleepTime % 1000000;
    if (sec > 0) {
        sleep(sec);
    }
    if (usec > 0) {
        usleep(usec);
    }
}

MyPacket* CreatePacket(struct timeval* prevTime, struct timeval* curTime, int packetsNeeded, int serviceTime) {
    systemStats.totalPacketNum++;
    
    char timestampStr[15];
    GenerateTraceTimestamp(timestampStr, curTime);
    long long curInterArrivalTime = timeDiffMicroSec(curTime, prevTime);
    systemStats.interArrivalTimeRunAvg = (double) (systemStats.interArrivalTimeRunAvg * (systemStats.totalPacketNum - 1) + curInterArrivalTime) / systemStats.totalPacketNum;
    
    if (packetsNeeded > B) {
        systemStats.droppedPacket++;
        printf("%s: p%d arrives, needs %d tokens, inter-arrival time = %.3fms, dropped\n", timestampStr, systemStats.totalPacketNum, packetsNeeded, curInterArrivalTime / 1000.0);
        return NULL;
    }

    MyPacket* packet = (MyPacket*) malloc(sizeof(MyPacket));
    packet->arriveSystemTime = *curTime;
    packet->packetNum = systemStats.totalPacketNum;
    packet->packetsNeeded = packetsNeeded;
    packet->serviceTime = serviceTime;
    printf("%s: p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n", timestampStr, packet->packetNum, packetsNeeded, curInterArrivalTime / 1000.0);
    return packet;
}

void SendPacketToQ1(MyPacket* packet) {
    struct timeval curTime;
    gettimeofday(&curTime, 0);

    packet->arriveQ1Time = curTime;
    My402ListAppend(&Q1, packet);

    char timestampStr[15];
    GenerateTraceTimestamp(timestampStr, &curTime);
    printf("%s: p%d enters Q1\n", timestampStr, packet->packetNum);
}

void GenerateTraceTimestamp(char* timestampStr, struct timeval* curTime) {
    memset(timestampStr, '0', 11);
    long long timeDiff = timeDiffMicroSec(curTime, &startTime);
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

void* GeneratingTokens(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    
    struct timeval prevTime = startTime;
    struct timeval curTime;
    while (TRUE) {
        
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
        SleepAdjustedAmountOfTime(interTokenArrivalTime * 1000, &prevTime);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
        
        pthread_mutex_lock(&mutex);

        if (shouldTerminate == 1) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        gettimeofday(&curTime, 0);
        InsertToken(&curTime);

        if (!My402ListEmpty(&Q1)) {
            MyPacket* headPacket = (MyPacket*) My402ListFirst(&Q1)->obj;
            if (headPacket->packetsNeeded == systemStats.bucketTokenNum) {
                SendPacketFromQ1ToQ2(headPacket);
                pthread_cond_broadcast(&cv);
            }
        }

        if (NoMorePacketsToCome()) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        pthread_mutex_unlock(&mutex);

        prevTime = curTime;
    }
    pthread_exit(0);
}

int InsertToken(struct timeval* curTime) {
    systemStats.totalTokenNum++;
    char timestampStr[15];
    GenerateTraceTimestamp(timestampStr, curTime);

    if (systemStats.bucketTokenNum == B) {
        systemStats.droppedToken++;
        printf("%s: token t%d arrives, dropped\n", timestampStr, systemStats.totalTokenNum);
        return FALSE;
    }

    systemStats.bucketTokenNum++;
    printf("%s: token t%d arrives, token bucket now has %d tokens\n", timestampStr, systemStats.totalTokenNum, systemStats.bucketTokenNum);
    return TRUE;
}

void SendPacketFromQ1ToQ2(MyPacket* packet) {
    My402ListUnlink(&Q1, My402ListFirst(&Q1));
    systemStats.bucketTokenNum -= packet->packetsNeeded;
    struct timeval curTime;
    gettimeofday(&curTime, 0);
    packet->leaveQ1Time = curTime;
    char timestampStr[15];
    GenerateTraceTimestamp(timestampStr, &packet->leaveQ1Time);
    long long timeInQ1 = timeDiffMicroSec(&packet->leaveQ1Time, &packet->arriveQ1Time);
    printf("%s: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d token\n", timestampStr, packet->packetNum, timeInQ1 / 1000.0, systemStats.bucketTokenNum);

    My402ListAppend(&Q2, packet);
    gettimeofday(&curTime, 0);
    packet->arriveQ2Time = curTime;
    GenerateTraceTimestamp(timestampStr, &packet->arriveQ2Time);
    printf("%s: p%d enters Q2\n", timestampStr, packet->packetNum);
}

int NoMorePacketsToCome() {
    return (systemStats.totalPacketNum == num && My402ListEmpty(&Q1)) || shouldTerminate == 1;
}

void* Server(void* arg) {
    int serverId = (int) arg;
    while (TRUE) {
        pthread_mutex_lock(&mutex);
        
        while (My402ListEmpty(&Q2) && !NoMorePacketsToCome()) {
            pthread_cond_wait(&cv, &mutex);
        }

        if (My402ListEmpty(&Q2) && NoMorePacketsToCome()) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        MyPacket* headPacket = (MyPacket*) My402ListFirst(&Q2)->obj;
        GetPacketFromQ2(headPacket, serverId);

        pthread_mutex_unlock(&mutex);

        usleep(headPacket->serviceTime * 1000);
        TransmitPacket(headPacket, serverId);
    }
    pthread_exit(0);
}

void GetPacketFromQ2(MyPacket* packet, int serverId) {
    My402ListUnlink(&Q2, My402ListFirst(&Q2));
    struct timeval curTime;
    gettimeofday(&curTime, 0);
    packet->leaveQ2Time = curTime;
    char timestampStr[15];
    GenerateTraceTimestamp(timestampStr, &packet->leaveQ2Time);
    long long timeInQ2 = timeDiffMicroSec(&packet->leaveQ2Time, &packet->arriveQ2Time);
    printf("%s: p%d leaves Q2, time in Q2 = %.3fms\n", timestampStr, packet->packetNum, timeInQ2 / 1000.0);

    gettimeofday(&curTime, 0);
    packet->arriveServerTime = curTime;
    GenerateTraceTimestamp(timestampStr, &packet->arriveServerTime);
    printf("%s: p%d begins service at S%d, requesting %dms of service\n", timestampStr, packet->packetNum, serverId, packet->serviceTime);
}

void TransmitPacket(MyPacket* packet, int serverId) {
    struct timeval curTime;
    gettimeofday(&curTime, 0);

    packet->leaveServerTime = curTime;
    char timestampStr[15];
    GenerateTraceTimestamp(timestampStr, &packet->leaveServerTime);
    long long timeInServer = timeDiffMicroSec(&packet->leaveServerTime, &packet->arriveServerTime);
    long long timeInSystem = timeDiffMicroSec(&packet->leaveServerTime, &packet->arriveSystemTime);
    long long timeInQ1 = timeDiffMicroSec(&packet->leaveQ1Time, &packet->arriveQ1Time);
    long long timeInQ2 = timeDiffMicroSec(&packet->leaveQ2Time, &packet->arriveQ2Time);
    printf("%s: p%d departs from S%d, service time = %.3fms, time in system = %.3fms\n", timestampStr, packet->packetNum, serverId, timeInServer / 1000.0, timeInSystem / 1000.0);

    systemStats.completedPacket++;
    systemStats.timeInQ1RunAvg = (double) (systemStats.timeInQ1RunAvg * (systemStats.completedPacket - 1) + timeInQ1) / systemStats.completedPacket;
    systemStats.timeInQ2RunAvg = (double) (systemStats.timeInQ2RunAvg * (systemStats.completedPacket - 1) + timeInQ2) / systemStats.completedPacket;
    systemStats.systemTimeRunAvg = (double) (systemStats.systemTimeRunAvg * (systemStats.completedPacket - 1) + timeInSystem) / systemStats.completedPacket;
    systemStats.systemTimeSqrRunAvg = (double) (systemStats.systemTimeSqrRunAvg * (systemStats.completedPacket - 1) + timeInSystem * timeInSystem) / systemStats.completedPacket;
    systemStats.serviceTimeRunAvg = (double) (systemStats.serviceTimeRunAvg * (systemStats.completedPacket - 1) + timeInServer) / systemStats.completedPacket;
    if (serverId == 1) {
        systemStats.departFromS1++;
        systemStats.timeInS1RunAvg = (double) (systemStats.timeInS1RunAvg * (systemStats.departFromS1 - 1) + timeInServer) / systemStats.departFromS1;
    }
    else {
        systemStats.departFromS2++;
        systemStats.timeInS2RunAvg = (double) (systemStats.timeInS2RunAvg * (systemStats.departFromS2 - 1) + timeInServer) / systemStats.departFromS2;
    }

    free(packet);
}

void* HandlingSignal(void* arg) {
    sigset_t* set = (sigset_t*) arg;
    int sig;
    sigwait(set, &sig);
    
    pthread_mutex_lock(&mutex);
    
    struct timeval curTime;
    char timestampStr[15];
    gettimeofday(&curTime, 0);
    GenerateTraceTimestamp(timestampStr, &curTime);
    printf("\n%s: SIGINT caught, no new packets or tokens will be allowed\n", timestampStr);
    shouldTerminate = 1;
    RemoveAllPackets();
    
    pthread_cancel(packetThread);
    pthread_cancel(tokenThread);
    pthread_cond_broadcast(&cv);

    pthread_mutex_unlock(&mutex);

    pthread_exit(0);
}

void RemoveAllPackets() {
    struct timeval curTime;
    char timestampStr[15];

    My402ListElem* curElem = My402ListFirst(&Q1);
    while (curElem != NULL) {
        My402ListElem* nextElem = My402ListNext(&Q1, curElem);
        MyPacket* packet = (MyPacket*) curElem->obj;
        gettimeofday(&curTime, 0);
        GenerateTraceTimestamp(timestampStr, &curTime);
        printf("%s: p%d removed from Q1\n", timestampStr, packet->packetNum);
        free(packet);
        free(curElem);
        curElem = nextElem;
    }
    My402ListInit(&Q1);

    curElem = My402ListFirst(&Q2);
    while (curElem != NULL) {
        My402ListElem* nextElem = My402ListNext(&Q2, curElem);
        MyPacket* packet = (MyPacket*) curElem->obj;
        gettimeofday(&curTime, 0);
        GenerateTraceTimestamp(timestampStr, &curTime);
        printf("%s: p%d removed from Q2\n", timestampStr, packet->packetNum);
        free(packet);
        free(curElem);
        curElem = nextElem;
    }
    My402ListInit(&Q2);
}

void PrintStats() {
    long long emulationTime = timeDiffMicroSec(&endTime, &startTime);
    
    printf("Statistics:\n");
    if (systemStats.totalPacketNum == 0) {
        printf("    average packet inter-arrival time = (N/A, no packet arrived)\n");
    }
    else {
        printf("    average packet inter-arrival time = %.6g\n", systemStats.interArrivalTimeRunAvg / 1000000);
    }
    if (systemStats.completedPacket == 0) {
        printf("    average packet service time = (N/A, no packet was served)\n");
        printf("    average number of packets in Q1 = 0\n");
        printf("    average number of packets in Q2 = 0\n");
        printf("    average number of packets in S1 = 0\n");
        printf("    average number of packets in S2 = 0\n");
        printf("    average time a packet spent in system = (N/A, no packet was served)\n");
        printf("    standard deviation for time spent in system = (N/A, no packet was served)\n");
    }
    else {
        printf("    average packet service time = %.6g\n\n", systemStats.serviceTimeRunAvg / 1000000);
        printf("    average number of packets in Q1 = %.6g\n", systemStats.timeInQ1RunAvg * systemStats.completedPacket / emulationTime);
        printf("    average number of packets in Q2 = %.6g\n", systemStats.timeInQ2RunAvg * systemStats.completedPacket / emulationTime);
        printf("    average number of packets in S1 = %.6g\n", systemStats.timeInS1RunAvg * systemStats.departFromS1 / emulationTime);
        printf("    average number of packets in S2 = %.6g\n\n", systemStats.timeInS2RunAvg * systemStats.departFromS2 / emulationTime);
        printf("    average time a packet spent in system = %.6g\n", systemStats.systemTimeRunAvg / 1000000);
        printf("    standard deviation for time spent in system = %.6g\n\n", sqrt(systemStats.systemTimeSqrRunAvg - systemStats.systemTimeRunAvg * systemStats.systemTimeRunAvg) / 1000000);
    }
    if (systemStats.totalTokenNum == 0) {
        printf("    token drop probability = (N/A, no token was generated)\n");
    }
    else {
        printf("    token drop probability = %.6g\n", (double) systemStats.droppedToken / systemStats.totalTokenNum);
    }
    if (systemStats.totalPacketNum == 0) {
        printf("    packet drop probability = (N/A, no packet arrived)\n");
    }
    else {
        printf("    packet drop probability = %.6g\n", (double) systemStats.droppedPacket / systemStats.totalPacketNum);
    }
}
