#include "cfgfile.h"


/////////////////////////begin: defination area of const///////////////////
#define MAX_SECTION_LENGTH 50     //max length of section
#define MAX_KEY_LENGTH 50         //max length of key
#define MAX_VALUE_LENGTH 1000    //max length of value  128->500
#define DEFAULT_KV_NUM_PER_SEC 50 //defalut num of key-value pairs per section
#define MAX_SECTION_NUM 50       //max num of sections

#define DEFAULT_LINE_NUM 50       //default num of lines

#define DEFAULT_BUF_LEN 1024
#define COMMENT_CHAR '#'
#define END_CHAR  0
///////////////////////// end : defination area of const/////////////////


/////////////////////////begin:defination area of type//////////
enum ELineTye
{
    LINE_TYPE_UNUSEFUL,
    LINE_TYPE_SEC_NAME,
    LINE_TYPE_KEY_VALUE
};
struct TLineInfo
{
    int iLineType;//ELineTye
    char SecName[MAX_SECTION_LENGTH];
    char Key[MAX_KEY_LENGTH];
    char Value[MAX_VALUE_LENGTH];
    char Content[DEFAULT_BUF_LEN];
};
/////////////////////////end :defination area of type/////////////////////


/////////////////////////begin:defination area of private function//////////
int _Preprocess();
void _DisplayLog(char* pMsg/*IN*/);
int _ReadLine(char* pLineBuf/*OUT*/);
bool _Init();
void _TrimSpace(char* pDesBuf/*IN OUT*/, int iMaxLen/*IN*/);
void _HandleLineBuf(char* pLine/*IN*/, int iMaxLen/*IN*/);
void _test_DisplayAll_KV();
bool _GetValue(char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*OUT*/);
bool _SetValue(char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*IN*/, bool bInsert);
int _ReplaceStr(char *pSrc/*IN OUT*/, char *pMatchStr/*IN*/, char *pReplaceStr/*IN*/, bool bReplaceAl/*IN*/);
int _InsertLineTable(struct TLineInfo* pLineTable/*IN*/, char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*IN*/);
/////////////////////////end of:defination area of private function////////////////////////////////


////////////////////////////////////////////////////////////////////////////
//defination of Global parameter which used only in cfgParam.cpp
static int g_bModifed = 0;//Define whether configue file is modified
static FILE* g_logfile = NULL;
static int g_bUseLog = 0;
static int g_iCurLinePos = 0;
static struct TLineInfo* g_LineTable = NULL;
static char g_CurSecName[MAX_SECTION_LENGTH];
////////////////////////////////////////////////////////////////////////////

int cfg_Open(char* pFileName/*IN*/)
{
    if(NULL == pFileName)
    {
        _DisplayLog("pointer of file name  is NULL ");
        return -1;
    }
    if(NULL != g_logfile)
    {
        _DisplayLog("logfile has been opened");
        return 2;
    }
    _Init();
    g_logfile = fopen(pFileName,"rw+");
    if(NULL == g_logfile)    
    {
        _DisplayLog("logfile open fail");
        return -2;
    }
    else
    {
        if(_Preprocess()<0)
        {
            _DisplayLog("Handle config file fail");
            return -3;
        }   
        _DisplayLog("logfile open suc");
        return 0;
    }
}

void cfg_Close()
{
    if(NULL != g_logfile)
    {
        fclose(g_logfile);
        g_logfile = NULL;
    }
}

void EnableLog(bool isDebug/*IN*/)
{
    g_bUseLog = isDebug;
}

bool _GetValue(char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*OUT*/)
{
    if(NULL == pKey || NULL == pValue)
        return false;

    char secName[MAX_SECTION_LENGTH];
    memset(secName,0,MAX_SECTION_LENGTH);
    (NULL == pSec || strlen(pSec)<=0) ? strcpy(secName,"DEFAULT") : strcpy(secName, pSec);        
    _TrimSpace(secName,MAX_SECTION_LENGTH);

    char key[MAX_KEY_LENGTH];
    memset(key,END_CHAR,MAX_KEY_LENGTH);
    strcpy(key,pKey);
    _TrimSpace(key,MAX_KEY_LENGTH);

    int bFindValue = 0;
    int iCurLinePos = 0;

    while(iCurLinePos < g_iCurLinePos)
    {
        if( (LINE_TYPE_KEY_VALUE != g_LineTable[iCurLinePos].iLineType) || (0 != strcmp(secName,g_LineTable[iCurLinePos].SecName)) )
        { 
            ++iCurLinePos;
            continue;
        }

        if(0 == strcmp(key, g_LineTable[iCurLinePos].Key))
        {
            bFindValue = true;
            strcpy(pValue, g_LineTable[iCurLinePos].Value);
            break;
        }
        ++iCurLinePos;
    }
    return bFindValue;
}

bool GetValue_str(char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*OUT*/)
{
    return _GetValue(pSec,pKey,pValue);
}

bool GetValue_int(char* pSec/*IN*/, char* pKey/*IN*/, int* pValue/*OUT*/)
{ 
	if(NULL == pValue)
		return false;
    bool bRes = false; 
    char value[MAX_VALUE_LENGTH];
    memset(value,END_CHAR,MAX_VALUE_LENGTH);
    bRes = _GetValue(pSec,pKey,value);
    if(bRes)
        *pValue = atoi(value);
    return bRes;
}

bool GetValue_double(char* pSec/*IN*/, char* pKey/*IN*/, double dValue/*OUT*/)
{
    bool bRes = false; 
    char value[MAX_VALUE_LENGTH];
    memset(value,END_CHAR,MAX_VALUE_LENGTH);
    bRes = _GetValue(pSec,pKey,value);
    if(bRes)
        dValue = atof(value);
    return bRes;
}

bool _SetValue(char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*IN*/, bool bInsert)
{
    if(NULL == pKey || NULL == pValue)
        return false;

    char secName[MAX_SECTION_LENGTH];
    memset(secName,END_CHAR,MAX_SECTION_LENGTH);
    (NULL == pSec || strlen(pSec)<=0) ? strcpy(secName,"DEFAULT") : strcpy(secName, pSec);        
    _TrimSpace(secName,MAX_SECTION_LENGTH);

    char key[MAX_KEY_LENGTH];
    memset(key,END_CHAR,MAX_KEY_LENGTH);
    strcpy(key,pKey);
    _TrimSpace(key,MAX_KEY_LENGTH);

    char value[MAX_VALUE_LENGTH];
    memset(value,END_CHAR,MAX_VALUE_LENGTH);
    strcpy(value,pValue);
    _TrimSpace(value,MAX_VALUE_LENGTH);

    bool bFindValue = false;
    int iCurLinePos = 0;
    while(iCurLinePos < g_iCurLinePos)
    {

        if( (LINE_TYPE_KEY_VALUE != g_LineTable[iCurLinePos].iLineType) || (0 != strcmp(secName,g_LineTable[iCurLinePos].SecName)) )
        { 
            ++iCurLinePos;
            continue;
        }

        if(0 == strcmp(key, g_LineTable[iCurLinePos].Key))
        {
            bFindValue = true;
            break;
        }
        ++iCurLinePos;
    }
    if(bFindValue)
    {
        _ReplaceStr(g_LineTable[iCurLinePos].Content,g_LineTable[iCurLinePos].Value, value,false);
        strcpy(g_LineTable[iCurLinePos].Value, value);
    }
    else
        if(bInsert)
        {

            _InsertLineTable(g_LineTable, secName, key, value);
        }
    g_bModifed = bFindValue;
    return bFindValue;

}

void SetValue_str(char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*IN*/)
{
    _SetValue(pSec, pKey, pValue,true);
}


void SetValue_int(char* pSec/*IN*/, char* pKey/*IN*/, int iValue/*IN*/)
{
    char value[MAX_VALUE_LENGTH];
    memset(value,END_CHAR,MAX_VALUE_LENGTH);
    sprintf(value,"%d",iValue);
    _SetValue(pSec, pKey, value,true);
}

void SetValue_double(char* pSec/*IN*/, char* pKey/*IN*/, double dValue/*IN*/)
{
    char value[MAX_VALUE_LENGTH];
    memset(value,END_CHAR,MAX_VALUE_LENGTH);
    sprintf(value,"%f",dValue);
    _SetValue(pSec, pKey, value,true);
}

int _InsertLineTable(struct TLineInfo* pLineTable/*IN*/, char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*IN*/)
{
    if(NULL == pLineTable || NULL == pKey || NULL == pValue)
        return -1;

    char secName[MAX_SECTION_LENGTH];
    memset(secName,END_CHAR,MAX_SECTION_LENGTH);
    (NULL == pSec || strlen(pSec)<=0) ? strcpy(secName,"DEFAULT") : strcpy(secName, pSec);        
    _TrimSpace(secName,MAX_SECTION_LENGTH);

    char key[MAX_KEY_LENGTH];
    memset(key,END_CHAR,MAX_KEY_LENGTH);
    strcpy(key,pKey);
    _TrimSpace(key,MAX_KEY_LENGTH);

    char value[MAX_VALUE_LENGTH];
    memset(value,END_CHAR,MAX_VALUE_LENGTH);
    strcpy(value,pValue);
    _TrimSpace(value,MAX_VALUE_LENGTH);

    char Content[DEFAULT_BUF_LEN];
    memset(Content,END_CHAR, DEFAULT_BUF_LEN);
    sprintf(Content,"%s = %s\n",key,value);

    char logmsg[128];
    sprintf(logmsg,"_InsertLineTable, will insert: %s",Content);
    _DisplayLog(logmsg);

    int iDesLinePos = 0;
    bool bFind = false;
    while(iDesLinePos < g_iCurLinePos)
    {
        if( 0 == strcmp(secName,g_LineTable[iDesLinePos].SecName) )
        { 
            bFind = true;
            break;
        }
        ++iDesLinePos;
    }
    ++iDesLinePos;
    if(bFind)
    {
    
        sprintf(logmsg,"_InsertLineTable: find section:%s,Line NO.:",secName,iDesLinePos);
        _DisplayLog(logmsg);
        int iStartMovePos = g_iCurLinePos;
        ++g_iCurLinePos;
        while(iStartMovePos >= iDesLinePos)
        {
            g_LineTable[iStartMovePos] = g_LineTable[iStartMovePos-1];
            --iStartMovePos;
        }
        sprintf(logmsg,"Insert Line No:%d",iStartMovePos);
        _DisplayLog(logmsg);
        g_LineTable[iStartMovePos].iLineType = LINE_TYPE_KEY_VALUE;
        strcpy(g_LineTable[iStartMovePos].Key, key);
        strcpy(g_LineTable[iStartMovePos].Value, value);
        strcpy(g_LineTable[iStartMovePos].SecName, secName); 
        strcpy(g_LineTable[iStartMovePos].Content, Content); 
    }
    else
    {sprintf(logmsg,"_InsertLineTable: cann't find section:%s",secName);
        _DisplayLog(logmsg);
        g_LineTable[g_iCurLinePos].iLineType = LINE_TYPE_KEY_VALUE;
        strcpy(g_LineTable[g_iCurLinePos].Key, key);
        strcpy(g_LineTable[g_iCurLinePos].Value, value);
        strcpy(g_LineTable[g_iCurLinePos].SecName, secName); 
        strcpy(g_LineTable[g_iCurLinePos].Content, Content); 
        ++g_iCurLinePos;
    }
}

void _DisplayLog(char* pMsg/*IN*/)
{
    if(g_bUseLog && NULL != pMsg)
    {
        printf("[cfgfile.cpp]: ");
        printf("%s",pMsg);
        printf("\n");
    }
}


int _Preprocess()
{
    if(NULL == g_logfile)
        return -1;
    if(NULL == g_LineTable)
        g_LineTable = (struct TLineInfo*)malloc(sizeof(struct TLineInfo) * DEFAULT_LINE_NUM);
    char LineBuf[DEFAULT_BUF_LEN];
    memset(LineBuf,END_CHAR,DEFAULT_BUF_LEN);
    while(0 != _ReadLine(LineBuf))
    {
        _HandleLineBuf(LineBuf,DEFAULT_BUF_LEN);
        memset(LineBuf,END_CHAR,DEFAULT_BUF_LEN);
    }
}


int _ReadLine(char* pLineBuf/*OUT*/)
{
    if(NULL == g_logfile || NULL == pLineBuf)
    {
        _DisplayLog("fail:pointer is NULL in _ReadLine()");
        return -1;
    }
    char* pRes = NULL;
    pRes = fgets(pLineBuf,DEFAULT_BUF_LEN,g_logfile);
    return (NULL == pRes ? 0 : 1);
}

bool _Init()
{
    g_bModifed = false;
    g_logfile = NULL;
    if(NULL == g_LineTable)
        g_LineTable = (struct TLineInfo*)malloc(sizeof(struct TLineInfo) * DEFAULT_LINE_NUM);
    memset(g_LineTable, 0, MAX_SECTION_LENGTH * sizeof(struct TLineInfo));
}


void _TrimSpace(char* pDesBuf/*IN OUT*/, int iMaxLen/*IN*/)
{
    if(NULL == pDesBuf)
        return;
    int iUsedPos = 0;
    char cCurChar;
	int iCurPos=0;
    for(iCurPos=0; iCurPos<iMaxLen; ++iCurPos)
    {
        cCurChar = *(pDesBuf+iCurPos);
        if('\0' == cCurChar)
            break;
        if(' ' != cCurChar && '\t' != cCurChar && '\n' != cCurChar && '\r' != cCurChar)
        {
            *(pDesBuf+iUsedPos) = *(pDesBuf+iCurPos);
            ++iUsedPos;
        }
    }
    memset(pDesBuf+iUsedPos,0,iMaxLen-iUsedPos);
}

void _HandleLineBuf(char* pLine/*IN*/, int iMaxLen/*IN*/)
{
    if(NULL == pLine || iMaxLen<=0 || NULL == g_LineTable)
        return;

    char secName[MAX_SECTION_LENGTH];
    memset(secName,END_CHAR,MAX_SECTION_LENGTH);
    char key[MAX_KEY_LENGTH];
    memset(key,END_CHAR,MAX_KEY_LENGTH);
    char value[MAX_VALUE_LENGTH];
    memset(value,END_CHAR,MAX_VALUE_LENGTH);

    int iSecPos = 0, iKeyPos = 0, iValuePos = 0,iCurLinePos = 0;
    int isSec = false,isValue = false;
    while(iCurLinePos  <   iMaxLen           && 
          COMMENT_CHAR != pLine[iCurLinePos] && 
          ']' != pLine[iCurLinePos] && 
          END_CHAR != pLine[iCurLinePos]) 
    {
        if(' ' == pLine[iCurLinePos])
        {
            ++iCurLinePos;
            continue;
        }
        if('[' == pLine[iCurLinePos])
        {
            isSec = true;
            ++iCurLinePos;
            continue;
        }
        if('=' == pLine[iCurLinePos])
        {
        	isValue = true;
            ++iCurLinePos;
            continue;
        }

        if(isSec)
        {
            secName[iSecPos] = pLine[iCurLinePos];
            ++iSecPos;
        }
        else
        {
            if(isValue)
            {
                value[iValuePos] = pLine[iCurLinePos];
                ++iValuePos;
            }
            else
            {
                key[iKeyPos] = pLine[iCurLinePos];
                ++iKeyPos;
            }
        }
        ++iCurLinePos;
    }
    if(isSec)
    {
        g_LineTable[g_iCurLinePos].iLineType = LINE_TYPE_SEC_NAME;
        _TrimSpace(secName, MAX_SECTION_LENGTH);
        strcpy(g_LineTable[g_iCurLinePos].SecName, secName);
        strcpy(g_CurSecName, secName);
    }else
         if(isValue)
        {
            g_LineTable[g_iCurLinePos].iLineType = LINE_TYPE_KEY_VALUE;
            _TrimSpace(key, MAX_KEY_LENGTH);
            _TrimSpace(value, MAX_VALUE_LENGTH);
            strcpy(g_LineTable[g_iCurLinePos].Key, key);
            strcpy(g_LineTable[g_iCurLinePos].Value, value);
            strcpy(g_LineTable[g_iCurLinePos].SecName, g_CurSecName);  
        }
        else
           g_LineTable[g_iCurLinePos].iLineType = LINE_TYPE_UNUSEFUL;
    strcpy(g_LineTable[g_iCurLinePos].Content,pLine);    
    ++g_iCurLinePos;  
}

void _test_DisplayAll_KV()
{
    int iCurLinePos = 0;
    _DisplayLog("will display all information of config file");

    char logmsg[128];
    sprintf(logmsg,"[_test_DisplayAll_KV] g_iCurLinePos = %d",g_iCurLinePos);
    _DisplayLog(logmsg);
    while(iCurLinePos < g_iCurLinePos)
    {
        sprintf(logmsg,"\n\nLineNo:%d; LineType%d; content: %s",iCurLinePos,g_LineTable[iCurLinePos].iLineType,g_LineTable[iCurLinePos].Content);
        _DisplayLog(logmsg);
        if(LINE_TYPE_SEC_NAME == g_LineTable[iCurLinePos].iLineType)
            sprintf(logmsg,"detail:section name %s",g_LineTable[iCurLinePos].SecName);
        if(LINE_TYPE_KEY_VALUE == g_LineTable[iCurLinePos].iLineType)
            sprintf(logmsg,"detail: key %s = value %s",g_LineTable[iCurLinePos].Key, g_LineTable[iCurLinePos].Value);

        _DisplayLog(logmsg);
        ++iCurLinePos;
    }
}


int _ReplaceStr(char* pSrc/*IN OUT*/, char* pMatchStr/*IN*/, char* pReplaceStr/*IN*/, bool bReplaceAll/*IN*/)
{
    if(NULL == pSrc || NULL == pMatchStr ||NULL == pReplaceStr )
        return -1;
    int  StringLen;
    char caNewString[DEFAULT_BUF_LEN];

    char *FindPos = strstr(pSrc, pMatchStr);
    if(NULL == FindPos)
        return -2;
    while(NULL != FindPos)
    {
        memset(caNewString, 0, sizeof(caNewString));
        StringLen = FindPos - pSrc;
        strncpy(caNewString, pSrc, StringLen);
        strcat(caNewString, pReplaceStr);
        strcat(caNewString, FindPos + strlen(pMatchStr));
        strcpy(pSrc, caNewString);
        if(!bReplaceAll)
            break;
        FindPos = strstr(pSrc, pMatchStr);
    }

    return 0;
}
