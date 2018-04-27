#include "../svm/sedona.h"
#include "misc/MTypes.h"
#include "initializer.h"

#define MIN_LINES_MALLOC 10
#define MIN_LINE_MALLOC 10

#define SPACE 32
#define NEWLINE 10
#define CARRIAGE_RETURN 13

// internal forwards
static u1* readFileRaw(u1* filename);
static u1** readFileLines(u1* filename);
static u1 isSpacer(u1 c);
static u1* substr(u1* str, size_t from, size_t count);
static int64_t indexOf(u1* str, u1 c);
static size_t countChar(u1* str, u1 c);
static u1** split(u1* str, u1 dil);
static u2* mku2(u1* str);
static size_t castToInt(u1* str);
static u1* u2Tou1(u2* s2);

void init_fileManager() {
    MFileManager->mku2          = mku2;
    MFileManager->readFileRaw   = readFileRaw;
    MFileManager->readFileLines = readFileLines;
    MFileManager->countChar     = countChar;
    MFileManager->indexOf       = indexOf;
    MFileManager->isSpacer      = isSpacer;
    MFileManager->split         = split;
    MFileManager->substr        = substr;
    MFileManager->castToInt     = castToInt;
    MFileManager->u2Tou1        = u2Tou1;
}

inline static u1* u2Tou1(u2* s2) {
    size_t len = 0;
    while(s2[len++] != NULL);

    u1* s1 = (u1*) malloc(len + 1);
    size_t start = 0;
    while(start < len) {
        s1[start] = (u1) s2[start];
        start++;
    }
    s1[start] = '\0';
    return s1;
}

inline static size_t castToInt(u1* str) {
    size_t result = 0;

    while(*str != NULL) {
        result *= 10;
        result += (*str - '0');
        str++;
    }
    return result;
 }

inline static u2* mku2(u1* str) {
    size_t len = strlen(str);
    size_t start = 0;

    u2* u2str = (u2*) malloc((len + 1) * sizeof(u2));
    while(start < len) {
        u2str[start] = (u2) str[start];
        start++;
    }

    u2str[start] = (u2) '\0';
    return u2str;
}

inline static size_t countChar(u1* str, u1 c) {
    size_t start = 0;
    size_t count = 0;
    size_t len   = strlen(str);

    while(start < len) {
        if(str[start++] == c) {
            count++;
        }
    }
    return count;
}

inline static u1** split(u1* str, u1 dil) {
    size_t arrayLen = countChar(str, dil) + 1;
    u1** splited = (u1**) malloc((arrayLen + 1) * sizeof(void*));

    size_t start = 0;
    u1* buff;
    size_t from = 0;
    int64_t to = 0;

    while(start < arrayLen) {
        to = indexOf(str + from, dil);
        if(to < 0) {
            to = strlen(str) - from;
        }
        buff = substr(str , from, to);
        from += to + 1;
        splited[start++] = buff;
    }

    splited[start] = '\0';
    return splited;
}

inline static u1* substr(u1* str, size_t from, size_t count) {
    u1* nstr = (u1*) malloc(count + 1);
    memcpy(nstr, str + from, count);
    nstr[count] = '\0';
    return nstr;
}

inline static int64_t indexOf(u1* str, u1 c) {
    u1* index = strchr(str, c);
    if(index == NULL) {
        return -1;
    }

    return (int64_t) (index - str);
}

inline static u1* readFileRaw(u1* filename) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL){
        printf("%s error fopen, no meta loaded!\n", filename);
        return NULL;
    }

    size_t result = fseek(file, 0, SEEK_END);
    if (result != 0) {
        printf("%s error file-seek!\n", filename);
        return NULL;
    }

    size_t size = ftell(file);
    rewind(file);
    u1* fBuff = (u1*) malloc(size + byte__SIZE);

    if(fBuff == NULL) {
        printf("%s error malloc\n", filename);
        return NULL;
    }

    result = fread(fBuff, 1, size, file);
    if(result != size) {
        printf("%s error fread!\n", filename);
        return NULL;
    }

    fclose(file);
    fBuff[size] = '\0';
    return fBuff;
}

inline static u1 isSpacer(u1 c) {
    if(c == SPACE || c == NEWLINE || c == CARRIAGE_RETURN)
        return TRUE;

    return FALSE;
}

inline static u1** readFileLines(u1* filename) {
    return split(readFileRaw(filename), NEWLINE);

    // This code was before i implemented the split function!
    // Highly optimized this by splitting the raw file!
    /*
    u1* fBuff = readFileRaw(filename);
    strBuilder* strbuilder = (strBuilder*) malloc(sizeof(strBuilder));
    strbuilder->size = 0;

    u1List* word = (u1List*) malloc(sizeof(u1List));
    word->size = 0;

    u1 c;
    size_t fOffset = 0;
    size_t size = strlen(fBuff);

    while(fOffset < size) {
        c = fBuff[fOffset];
        if(isSpacer(c) == FALSE) {
            if(word->size == 0) {
                word->head = (u1Buff*) malloc(sizeof(u1Buff));
                word->head->u1 = c;
            }
            else {
                u1Buff* u1buff = (u1Buff*) malloc(sizeof(u1Buff));
                u1buff->u1 = c;
                u1buff->next = word->head;
                word->head = u1buff;
            }

            word->size++;
        }
        else { // dilimter
            if(word->size == 0) {
                fOffset++;
                continue;
            }
            else if(strbuilder->size == 0) {
                strbuilder->head = (u1ListBuff*) malloc(sizeof(u1ListBuff));
                strbuilder->head->word = word;
            }
            else {
                u1ListBuff* wordBuff = (u1ListBuff*) malloc(sizeof(u1ListBuff));
                wordBuff->word = word;
                wordBuff->next = strbuilder->head;
                strbuilder->head = wordBuff;
            }

            word = (u1List*) malloc(sizeof(u1List));
            word->size = 0;
            strbuilder->size++;
        }

        fOffset++;
    }

    free(fBuff);
    free(word);

    size_t metaTokenCount = strbuilder->size;
    u1** tokenList = (u1**) malloc((metaTokenCount + 1) * sizeof(void*));
    size_t tokenListIndex = metaTokenCount - 1;

    size_t outc = 0;

    u1ListBuff* wlist = strbuilder->head;
    u1Buff* wordChars;
    u1* pword;
    int16_t pword_len; // 16 bits enough tho

    while(outc++ < metaTokenCount) {
        wordChars           = wlist->word->head;
        pword_len           = wlist->word->size;
        pword               = (u1*) malloc(pword_len + 1);
        pword[pword_len]    = '\0';

        while(--pword_len >= 0) {
            pword[pword_len] = wordChars->u1;
            wordChars        = wordChars->next;
        }

        tokenList[tokenListIndex--] = pword;
        wlist = wlist->next;
    }

    tokenList[metaTokenCount] = '\0';
    free(strbuilder);
    return tokenList;*/
}

/*
 // still buggy
inline static u1** readFileLines(u1* filename) {
    u1* file = readFileRaw(filename);

    u2 lineCharAllocation = MIN_LINE_MALLOC;
    u1* line = (u1*) malloc(lineCharAllocation);

    size_t linesAllocated = MIN_LINES_MALLOC;
    u1** lines = (u1**) malloc(linesAllocated * sizeof(u1*));

    u2 lineCharCount = 0;
    size_t linesCount = 0;
    size_t buffCounter = 0;

    u1 ch = file[buffCounter++];
    while(ch != '\0') {
        while(ch != NEWLINE) {
            if (lineCharCount >= lineCharAllocation) {
                lineCharAllocation <<= 1;
                line = (u1*) realloc(line, lineCharAllocation);
            }

            printf("%c", ch);
            line[lineCharCount++] = ch;
            ch = file[buffCounter++];
        }

        if(linesCount >= linesAllocated) {
            linesAllocated <<= 1;
            lines = (u1**) realloc(lines, linesAllocated * sizeof(u1*));
        }

        lines[linesCount] = (u1*) malloc(lineCharCount + 1);
        memcpy(lines[linesCount], line, lineCharCount);

        lines[linesCount][lineCharCount] = '\0';
        lineCharCount = 0;
        linesCount++;
    }

    free(file);
    return lines;
}*/


