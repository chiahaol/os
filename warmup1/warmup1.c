#include <ctype.h> 
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "cs402.h"

#include "my402list.h"

#define MAX_LENGTH 1024
#define NUM_FIELDS 4
#define DATE_FIELD_LENGTH 15
#define DESCRIPTION_FIELD_LENGTH 24
#define DOLLAR_STRING_LENGTH 14
#define MAX_DOLLAR_AMOUNT 10000000

typedef struct MyTransaction {
    time_t timestamp;
    long long cents;
    char description[MAX_LENGTH + 1];
} MyTransaction;

static int inputDataNum = 0;

FILE* OpenFile(char*);
void BuildList(My402List*, FILE*);
int ParseInputLine(char*, char**);
int ValidateTransactionType(char*);
int ValidateTransactionTimestamp(My402List*, char*);
int ValidateTransactionAmount(char*);
int ValidateTransactionDescription(char*);
void InitMyTransaction(MyTransaction*, char**);
void QuickSortList(My402List*, My402ListElem*, int);
void UnlinkListElem(My402ListElem*);
void InsertListElemAfter(My402ListElem*, My402ListElem*);
void PrintMyList(My402List*);
void CentsToDollarString(long long, char*);

int main(int argc, char* argv[])
{   
    if (argc == 1 || argc > 3) {
        fprintf(stderr, "Error: illegal number of arguments (%d)!!\n", argc);
        fprintf(stderr, "Usage: warmup1 sort [tfile]\n");
        exit(-1);
    }
    else if (strcmp(argv[1], "sort") != 0) {
        fprintf(stderr, "Error: unknown argument \"%s\"!!\n", argv[1]);
        fprintf(stderr, "Usage: warmup1 sort [tfile]\n");
        exit(-1);
    }
    
    FILE* fp = (argc == 3) ? OpenFile(argv[2]) : stdin; 

    My402List list;
    memset(&list, 0, sizeof(My402List));
    My402ListInit(&list);
    BuildList(&list, fp);
    
    QuickSortList(&list, My402ListFirst(&list), list.num_members);
    PrintMyList(&list);

    My402ListUnlinkAll(&list);
    fclose(fp);

    return 0;
}

FILE* OpenFile(char* fileName) {
    if (opendir(fileName) != NULL) {
        fprintf(stderr, "Error: given file path is a directory (%s)!!\n", fileName);
        exit(-1);
    }
    FILE* fp = fopen(fileName, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file (%s)!!\n", fileName);
        exit(-1);
    }
    return fp;
}

void BuildList(My402List* list, FILE* fp) {
    char line[MAX_LENGTH];
    while (fgets(line, MAX_LENGTH + 10, fp) != NULL) {
        MyTransaction* transaction = (MyTransaction*) malloc(sizeof(MyTransaction));
        char* fields[NUM_FIELDS];
        ParseInputLine(line, fields);
        ValidateTransactionType(fields[0]);
        ValidateTransactionTimestamp(list, fields[1]);
        ValidateTransactionAmount(fields[2]);
        ValidateTransactionDescription(fields[3]);
        InitMyTransaction(transaction, fields);
        My402ListAppend(list, (void*) transaction);
        for (int i = 0; i < NUM_FIELDS; i++) {
            free(fields[i]);
        }
    }
    if (list->num_members == 0) {
        fprintf(stderr, "Error: should have at least one transaction record!!\n");
        exit(-1);
    }
}

int ParseInputLine(char* line, char** fields) {
    inputDataNum++;
    if (strlen(line) > MAX_LENGTH) {
        fprintf(stderr, "Error in input data %d: line length exceeds 1024 characters!!\n", inputDataNum);
        exit(-1);
    }
    line[strlen(line) - 1] = '\0';  // remove ending '\n'
    
    char* token = strtok(line, "\t");
    int count = 0;
    while (token != NULL && count < NUM_FIELDS) {
        fields[count] = (char*) malloc(strlen(token) + 1);
        strncpy(fields[count], token, strlen(token) + 1);
        token = strtok(NULL, "\t");
        count++;
    }

    if (token != NULL || count < NUM_FIELDS) {
        fprintf(stderr, "Error in input data %d: number of fields must be 4!!\n", inputDataNum);
        exit(-1);
    }

    return TRUE;
}

int ValidateTransactionType(char* type) {
    if (strcmp(type, "+") != 0 && strcmp(type, "-") != 0) {
        fprintf(stderr, "Error in input data %d: illegal transaction type (%s)!!\n", inputDataNum, type);
        exit(-1);
    }
    return TRUE;
}

int ValidateTransactionTimestamp(My402List* list, char* timestamp) {
    unsigned timestampLen = strlen(timestamp);
    if (timestampLen == 0 || timestampLen >= 11) {
        fprintf(stderr, "Error in input data %d: transaction timestamp has illegal length (%s)!!\n", inputDataNum, timestamp);
        exit(-1);
    }
    for (int i = 0; i < timestampLen; i++) {
        if (!isdigit(timestamp[i])) {
            fprintf(stderr, "Error in input data %d: transaction timestamp has illegal character (%s)!!\n", inputDataNum, timestamp);
            exit(-1);
        }
    }
    if (timestamp[0] == '0') {
        fprintf(stderr, "Error in input data %d: first digit of transaction timestamp can't be 0 (%s)!!\n", inputDataNum, timestamp);
        exit(-1);
    }
    char* endPtr;
    time_t t = (time_t) strtoul(timestamp, &endPtr, 10);
    time_t curTime = time(NULL);
    if (t >= curTime) {
        fprintf(stderr, "Error in input data %d: transaction timestamp can't be greater than current time (%s)!!\n", inputDataNum, timestamp);
        exit(-1);
    }
    My402ListElem* curElem = My402ListFirst(list);
    while (curElem != NULL) {
       if (((MyTransaction*) curElem->obj)->timestamp == t) {
            fprintf(stderr, "Error in input data %d: transaction records can't contain duplicate timestamp (%s)!!\n", inputDataNum, timestamp);
            exit(-1);
       }
       curElem = My402ListNext(list, curElem);
    }
    return TRUE;
}

int ValidateTransactionAmount(char* amount) {
    unsigned amountLen = strlen(amount);
    char* ptrDecimal = strchr(amount, '.');
    if (ptrDecimal == NULL) {
        fprintf(stderr, "Error in input data %d: transaction amount missing decimal point (%s)!!\n", inputDataNum, amount);
        exit(-1);
    }
    int decimalPos = ptrDecimal - amount;
    if ((amountLen - decimalPos - 1) != 2 || decimalPos == 0) {
        fprintf(stderr, "Error in input data %d: transaction amount has wrong decimal point position (%s)!!\n", inputDataNum, amount);
        exit(-1);
    }
    for (int i = 0; i < amountLen; i++) {
        if (i == decimalPos) continue;
        if (!isdigit(amount[i])) {
            fprintf(stderr, "Error in input data %d: transaction amount has illegal character (%s)!!\n", inputDataNum, amount);
            exit(-1);
        }
    }
    int firstNotZero = -1;
    for (int i = 0; i < amountLen; i++) {
        if (i == decimalPos) continue;
        if (amount[i] != '0') {
            firstNotZero = i;
            break;
        }
    }
    if (firstNotZero == -1) {
        fprintf(stderr, "Error in input data %d: transaction amount has to be positive (%s)!!\n", inputDataNum, amount);
        exit(-1);
    }
    if (firstNotZero > 0 && firstNotZero < decimalPos) {
        fprintf(stderr, "Error in input data %d: transaction amount can't have leading zero (%s)!!\n", inputDataNum, amount);
        exit(-1);
    }
    if (firstNotZero == 0 && decimalPos > 7) {
        fprintf(stderr, "Error in input data %d: transaction amount exceeds digit limit (%s)!!\n", inputDataNum, amount);
        exit(-1);
    }
    return TRUE;
}

int ValidateTransactionDescription(char* description) {
    unsigned startPos = 0;
    unsigned descriptionLen = strlen(description);
    while (startPos < descriptionLen && description[startPos] == ' ') {
        startPos++;
    }
    if (startPos == descriptionLen) {
        fprintf(stderr, "Error in input data %d: transaction description can't be empty (%s)!!\n", inputDataNum, description);
        exit(-1);
    }
    if (startPos > 0) {
        char tmp[descriptionLen - startPos + 1];
        strncpy(tmp, description + startPos, descriptionLen - startPos + 1);
        memset(description, '\0', descriptionLen + 1);
        strncpy(description, tmp, descriptionLen - startPos);
    }
    return TRUE;
}

void InitMyTransaction(MyTransaction* transaction, char** fields) {
    char* endPtr;
    time_t timestamp = (time_t) strtoul(fields[1], &endPtr, 10);
    
    unsigned amountLen = strlen(fields[2]);
    char* wholeNum = (char*) malloc(amountLen - 2);
    char* fractional = (char*) malloc(3);
    strncpy(wholeNum, fields[2], amountLen - 3);
    wholeNum[amountLen - 3] = '\0';
    strncpy(fractional, fields[2] + amountLen - 2, 3);
    int cents = atoi(wholeNum) * 100 + atoi(fractional);
    if (fields[0][0] == '-') {
        cents *= -1;
    }

    transaction->timestamp = timestamp;
    transaction->cents = cents;
    strncpy(transaction->description, fields[3], strlen(fields[3]) + 1);
}

void QuickSortList(My402List* list, My402ListElem* head, int numElem) {
    if (numElem < 2) return;

    My402ListElem* prev = head->prev;
    My402ListElem* lowPtr = prev;
    My402ListElem* curElem = My402ListNext(list, head);
    UnlinkListElem(head);
    time_t headTimestamp = ((MyTransaction*) head->obj)->timestamp;
    int lowNum = 0;
    for (int curPos = 0; curPos < numElem - 1; curPos++) {
        My402ListElem* nextElem = My402ListNext(list, curElem);
        MyTransaction* curTransaction = (MyTransaction*) curElem->obj;
        if (curTransaction->timestamp < headTimestamp) {
            UnlinkListElem(curElem);
            InsertListElemAfter(lowPtr, curElem);
            lowPtr = My402ListNext(list, lowPtr);
            lowNum++;
        }
        curElem = nextElem;
    }
    InsertListElemAfter(lowPtr, head);

    QuickSortList(list, prev->next, lowNum);
    QuickSortList(list, head->next, numElem - lowNum - 1);
}

void UnlinkListElem(My402ListElem* elem) {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
}

void InsertListElemAfter(My402ListElem* prev, My402ListElem* elem) {
    elem->next = prev->next;
    prev->next->prev = elem;
    prev->next = elem;
    elem->prev = prev;
}

void PrintMyList(My402List* list) {
    printf("+-----------------+--------------------------+----------------+----------------+\n");
    printf("|       Date      | Description              |         Amount |        Balance |\n");
    printf("+-----------------+--------------------------+----------------+----------------+\n");

    long long curBalance = 0;
    My402ListElem* curElem = My402ListFirst(list);
    while (curElem != NULL) {
        MyTransaction* transaction = (MyTransaction*) curElem->obj;
        char date[DATE_FIELD_LENGTH + 1];
        char description[DESCRIPTION_FIELD_LENGTH + 1];
        char amount[DOLLAR_STRING_LENGTH + 1];
        char balance[DOLLAR_STRING_LENGTH + 1];
        
        curBalance += transaction->cents;

        char* ctimeDate = ctime(&transaction->timestamp);
        strncpy(date, ctimeDate, 11);
        strncpy(date + 11, ctimeDate + 20, 4);
        date[DATE_FIELD_LENGTH] = '\0';

        unsigned descriptionLen = min(strlen(transaction->description), DESCRIPTION_FIELD_LENGTH);
        strncpy(description, transaction->description, descriptionLen);
        memset(description + descriptionLen, ' ', DESCRIPTION_FIELD_LENGTH - descriptionLen);
        description[DESCRIPTION_FIELD_LENGTH] = '\0';

        CentsToDollarString(transaction->cents, amount);
        CentsToDollarString(curBalance, balance);

        printf("| %s | %s | %s | %s |\n", date, description, amount, balance);
        
        curElem = My402ListNext(list, curElem);
    }
    printf("+-----------------+--------------------------+----------------+----------------+\n");
}

void CentsToDollarString(long long cents, char* str) {
    memset(str, ' ', DOLLAR_STRING_LENGTH);
    str[DOLLAR_STRING_LENGTH] = '\0';
    if (cents < 0) {
        str[0] = '(';
        str[DOLLAR_STRING_LENGTH - 1] = ')';
        cents *= -1;
    }
    
    int fractional = cents % 100;
    long long wholeNum = cents / 100;
    
    char overflow[] = "?,???,???.??";
    if (wholeNum >= MAX_DOLLAR_AMOUNT) {
        strncpy(str + 1, overflow, 12);
        return;
    }
    
    char defaultVal[] = "0.00";
    strncpy(str + 9, defaultVal, 4);
    
    char buff[4];
    snprintf(buff, 4, "%d", fractional);
    strncpy(str + DOLLAR_STRING_LENGTH - 1 - strlen(buff), buff, strlen(buff));
    
    char* printHead = str + 10;
    while (wholeNum > 0) {
        snprintf(buff, 4, "%lld", wholeNum % 1000);
        wholeNum /= 1000;
        printHead -= 3;
        strncpy(printHead + 3 - strlen(buff), buff, strlen(buff));
        if (wholeNum > 0) {
            memset(printHead, '0', 3 - strlen(buff));
            printHead -= 1;
            *printHead = ',';
        }
    }
}
