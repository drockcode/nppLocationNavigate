//this file is part of notepad++
//Copyright (C) 2011 AustinYoung<pattazl@gmail.com>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "PluginDefinition.h"
#include <shlwapi.h>
#include "DockingFeature/LNhistoryDlg.h"
#include <string.h>
#include <tchar.h>
#include <list>
#include <mutex>
#include <WinBase.h>

////////////////SELF DATA BEGIN///////////
TCHAR currFile[MAX_PATH] = {0};
static int currBufferID = -1, PrePositionNumber = -1;
static bool ready = false;
static bool haveDeleted = false;
static bool PageActive = false;
static bool isOpenFile = false;
static bool isAutoModify = false;
vector<MarkData> MarkHistory;
// ??? MarkType ??
int MarkTypeArr[] =
{
    SC_MARK_BACKGROUND,
    SC_MARK_UNDERLINE,
    SC_MARK_FULLRECT,
    SC_MARK_ROUNDRECT,
    SC_MARK_CIRCLE,
    SC_MARK_ARROW,
    SC_MARK_SHORTARROW
};
///////DEBUG//////////////////////////////////////
//TCHAR buffer[800]={0};
//wsprintf(buffer,TEXT("%d %d %d %d-%d %d %d %d-%d %d %d %d-%d %d %d %d-%d %d %d"), notifyCode->position,
//       notifyCode->ch,
//       notifyCode->modifiers,
//       ModifyType,
//       notifyCode->length,
//       notifyCode->linesAdded,
//       notifyCode->message,
//       notifyCode->line,
//       notifyCode->foldLevelNow,
//       notifyCode->foldLevelPrev,
//       notifyCode->margin,
//       notifyCode->listType,
//       notifyCode->x,
//       notifyCode->y,
//       notifyCode->token,
//       notifyCode->annotationLinesAdded,
//       notifyCode->nmhdr.code,
//       notifyCode->nmhdr.hwndFrom,
//       notifyCode->nmhdr.idFrom);
//::MessageBox(NULL,buffer , TEXT(""), MB_OK);
////////////////SELF DATA END///////////

extern FuncItem funcItem[nbFunc];
extern NppData nppData;

extern LocationNavigateDlg _LNhistory;

#define NEWFILE TEXT("new  1")
/////////// SELF FUNCTION BEGIN //////
static bool AllCloseFlag = false;
TCHAR currTmpFile[MAX_PATH];// ?????????
int currTmpBufferID; // ????????ID

enum ActionType
{
    ActionModify = 0,
    ActionActive,
    ActionClosed,
    ActionLocation,
};

struct ActionData
{
    ActionType type;
    int position;
    int length;
    bool changed;
};

list<ActionData> ActionDataList;
mutex ActionDataLock;

void DoFilesCheck()
{
    // ????????????????,????????,????????
    TCHAR  **buffer;
    long filecount = ::SendMessage( nppData._nppHandle, NPPM_GETNBOPENFILES, 0,
                                    ( LPARAM )ALL_OPEN_FILES );
    //TCHAR TMP[200];
    //wsprintf(TMP,TEXT("%d,%d,%d"),filecount);
    //::MessageBox(NULL,TMP , TEXT(""), MB_OK);
    buffer = new TCHAR*[filecount];

    for ( int i = 0; i < filecount; i++ )
        buffer[i] = new TCHAR[MAX_PATH];

    ::SendMessage( nppData._nppHandle, NPPM_GETOPENFILENAMES, ( WPARAM )buffer,
                   ( LPARAM )filecount );
    //for (int i=0;i<filecount;i++)
    //{
    //  ::MessageBox(NULL,buffer[i] , TEXT(""), MB_OK);
    //}
    // ????????????????
    long PreBufferID = -1;
    bool haveFiles = false;
    //EnterCriticalSection(&criCounter);

    for ( size_t listIndex = 0; listIndex < LocationList.size(); listIndex++ )
    {
        if ( LocationList[listIndex].bufferID != PreBufferID )
        {
            // ???????
            //int len = lstrlen( LocationList[listIndex].FilePath );
            haveFiles = false;
            PreBufferID = LocationList[listIndex].bufferID;

            for ( int ifile = 0; ifile < filecount; ifile++ )
            {
                if ( lstrcmp( buffer[ifile], LocationList[listIndex].FilePath ) == 0 )
                {
                    haveFiles = true;
                    break;
                }
            }
        }

        // ?????????
        if ( !haveFiles )
        {
            LocationList.erase( LocationList.begin() + listIndex );

            if ( LocationPos >= (long)listIndex )
                LocationPos--;

            if ( LocationPos < 0 )
                LocationPos = 0;

            //????????????
            listIndex--;
        }
    }

    for ( int i = 0; i < filecount; i++ )
        delete []buffer[i];

    delete []buffer;

    //_LNhistory.refreshDlg();
}

void DoModify( int len, int pos )
{
    // SEARCH in files WILL CHANGE IT
    //TCHAR buffer[10000];
    //wsprintf(buffer,TEXT("position=%d,length=%d,%d=%d,%d"),pos,len,notifyCode->nmhdr.idFrom,currBufferID,curScintilla);
    //::MessageBox(NULL,buffer, TEXT(""), MB_OK);
    bool needRemove = false;

    for ( size_t i = 0; i < LocationList.size(); i++ )
    {
        if ( LocationList[i].bufferID == currBufferID )
        {
            if ( len > 0 )
            {
                if ( LocationList[i].position > pos )
                    LocationList[i].position += len;
            }
            else
            {
                long lastpos = pos - len;

                if ( LocationList[i].position > pos && LocationList[i].position < lastpos )
                {
                    // ??????????,???
                    //??
                    LocationList[i].position = -1;
                    needRemove = true;
                }
                else if ( LocationList[i].position >= lastpos )
                    LocationList[i].position += len;
            }
        }
    }

    if ( needRemove )
    {
        for ( size_t i = 0; i < LocationList.size(); i++ )
        {
            if ( -1 == LocationList[i].position )
            {
                LocationList.erase( LocationList.begin() + i );

                // LocationPos ??
                if ( LocationPos >= (long)i )
                    LocationPos--;

                if ( LocationPos < 0 )
                    LocationPos = 0;

                //????????????
                i--;
            }
        }

        haveDeleted = true;
    }

    // ?????,????????
    if ( !haveDeleted )
    {
        int lastPos =  LocationList.size() - 1;

        if ( lastPos >= 0 )
            LocationList[lastPos].changed = true;
    }
}

void AddListData( LocationInfo *tmp )
{
    long preLen = LocationList.size();

    if ( LocationPos != preLen - 1 && !AlwaysRecord && !PageActive )
    {

        // ??????,?????? LocationPos ?????
        for ( int i = LocationPos + 1; i < preLen; i++ )
        {
            // ?????
            LocationList.pop_back();
        }
    }

    PageActive = false;
    //  ??????? volatile
    long lastIndex = LocationList.size() - 1;

    if ( lastIndex > 0 )
    {
        if ( LocationList[lastIndex].bufferID == ( *tmp ).bufferID &&
                //LocationList[lastIndex].position == (*tmp).position
                abs( LocationList[lastIndex].position - ( *tmp ).position ) < MaxOffset
           )
        {
            // ??????????
            return;
        }
    }

    //TCHAR buffer[500]={0};
    //wsprintf(buffer,TEXT("%d"),lastIndex);
    //::MessageBox(nppData._nppHandle, buffer, TEXT(""), MB_OK);
    LocationList.push_back( *tmp );

    while ( true )
    {
        if ( LocationList.size() > (size_t)MaxList )
        {
            // ???????
            LocationList.pop_front();
        }
        else
        {
            // ???,??????????
            break;
        }
    }

    LocationPos = LocationList.size() - 1;
}

void AddList( bool flag )
{
    if ( !bAutoRecord && !flag )
        return;

    long position = 0; //,col=0;
    //line = ::SendMessage(handle, NPPM_GETCURRENTLINE, 0, 0);
    // Get the current scintilla
    //int which = -1;
    //::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    //if (which == -1)
    //  return;
    //HWND curScintilla = (which == 0)?nppData._scintillaMainHandle:nppData._scintillaSecondHandle;

    position = ::SendMessage( curScintilla, SCI_GETCURRENTPOS, 0, 0 );

    // ????????,?????????,???? !bufferChanged &&
    if ( PrePositionNumber == position )
        return;

    //col = ::SendMessage(handle, NPPM_GETCURRENTCOLUMN, 0, 0);
    if ( PositionSetting && !flag )
    {
        // ??????,???????,?????LocationPos????
        PositionSetting = false;
        PrePositionNumber =  LocationList[LocationPos].position;
        return;
    }

    long len = LocationList.size();
    long prepos = 0;

    if ( len == 0 || LocationPos == -1 || LocationPos > len - 1 )
        prepos = PrePositionNumber;
    else
        prepos = LocationList[LocationPos].position;

    if ( abs( prepos - position ) > MaxOffset || PrePositionNumber == -1 )
    {
        //int currentEdit;
        //TCHAR path[MAX_PATH];
        //::SendMessage(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)path);
        //::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
        //EnterCriticalSection(&criCounter);
        //LocationInfo tmp;
        //tmp.position = position;
        //tmp.bufferID = currBufferID;
        bool tmpChanged;

        if ( haveDeleted )
        {
            //tmp.changed  = true;
            tmpChanged = true;
            haveDeleted = false;
        }
        else
            //tmp.changed  = false;
            tmpChanged = false;

        //lstrcpy(tmp.FilePath,currFile);
        ActionData actTmp;
        actTmp.type = ActionLocation;
        actTmp.position = position;
        actTmp.changed = tmpChanged;
        ActionDataLock.lock();
        ActionDataList.push_back( actTmp );
        ActionDataLock.unlock();
        //AddListData(&tmp);
        //LeaveCriticalSection(&criCounter);
        //_LNhistory.refreshDlg();
    }

    PrePositionNumber = position;
    PositionSetting = false;
}

const int _MARK_BOOKMARK = 1 << 24;

void InitBookmark()
{
    if ( !NeedMark || isAutoModify )
        return;

    // ???????,????
    if ( ByBookMark == MarkBookmark )
        return;

    // ??????????
    HWND ScintillaArr[] = {nppData._scintillaMainHandle, nppData._scintillaSecondHandle};

    // ?????????
    for ( int i = 0; i < 2; i++ )
    {
        HWND tmpScintilla = ScintillaArr[i];
        //nppData._scintillaMainHandle:nppData._scintillaSecondHandle;
        ::SendMessage( tmpScintilla, SCI_SETMARGINTYPEN, _MARK_INDEX,
                       SC_MARGIN_SYMBOL );
        // ?????????
        ::SendMessage( tmpScintilla, SCI_SETMARGINTYPEN, _SAVE_INDEX,
                       SC_MARGIN_SYMBOL );
        // ??????????,???0
        int widthMargin = 0;
        int SC_MarkType = SC_MARK_BACKGROUND;

        if (   ByBookMark == MarkUnderLine )
            widthMargin = 1; // ??????,?????BUG
        else if (   ByBookMark == MarkRect )
            widthMargin = 4;
        else if (
            ByBookMark == MarkRoundrect
            || ByBookMark == MarkCircle
            || ByBookMark == MarkTriangle
            || ByBookMark == MarkArrow
        )
            widthMargin = 12;

        ::SendMessage( tmpScintilla, SCI_SETMARGINWIDTHN, 4, widthMargin );

        int OriMask = ::SendMessage( tmpScintilla, SCI_GETMARGINMASKN, 4, 0 );
        int markerMask1 = ( 1 << _MARK_INDEX );
        int markerMask2 = ( 1 << _SAVE_INDEX );
        int tmpMask = 0;
        tmpMask = OriMask | markerMask1 | markerMask2;

        if ( ByBookMark > MarkUnderLine )
        {
            // ??????
            ::SendMessage( tmpScintilla, SCI_MARKERSETFORE, _MARK_INDEX,
                           MarkColor ); // ????????????
            ::SendMessage( tmpScintilla, SCI_MARKERSETFORE, _SAVE_INDEX,
                           SaveColor ); // ????????????
        }

        // set mask to exclude marker
        ::SendMessage( tmpScintilla, SCI_SETMARGINMASKN, 4, tmpMask );

        ::SendMessage( tmpScintilla, SCI_MARKERSETBACK, _MARK_INDEX,
                       MarkColor ); // ????DefaultColor
        ::SendMessage( tmpScintilla, SCI_MARKERSETBACK, _SAVE_INDEX,
                       SaveColor ); // ????DefaultColor
        ::SendMessage( tmpScintilla, SCI_MARKERDEFINE, _MARK_INDEX, SC_MarkType );
        ::SendMessage( tmpScintilla, SCI_MARKERDEFINE, _SAVE_INDEX, SC_MarkType );

        // SC_MARK_FULLRECT,SC_MARK_UNDERLINE ,SC_MARK_CIRCLE,SC_MARK_ARROW, SC_MARK_SMALLRECT,
    }
}

static bool ThreadNeedRefresh = false;

DWORD WINAPI ThreadFunc( LPVOID /*lpParam*/ )
{
    while ( !AllCloseFlag )
    {
        ActionDataLock.lock();
        while ( !ActionDataList.empty() )
        {
            if ( !ThreadNeedRefresh )
                ThreadNeedRefresh = true;

            ActionData tmpAct = ActionDataList.front();
            ActionDataLock.unlock();

            switch ( tmpAct.type )
            {
                case ActionModify :
                    // ???
                    DoModify( tmpAct.length, tmpAct.position );
                    break;

                case ActionActive :
                {
                    // ????,???????
                    currBufferID =  currTmpBufferID;
                    lstrcpy( currFile, currTmpFile );

                    // ?????? bufferID=0??
                    for ( size_t i = 0; i < LocationList.size(); i++ )
                    {
                        if ( LocationList[i].bufferID == 0
                                && lstrcmp( LocationList[i].FilePath, currFile ) == 0 )
                            LocationList[i].bufferID = currBufferID;
                    }

                    InitBookmark();
                }
                break;

                case ActionClosed :
                    // ??????
                    DoFilesCheck();
                    break;

                case ActionLocation :
                {
                    // ????
                    LocationInfo tmp;
                    tmp.position = tmpAct.position;
                    tmp.bufferID = currBufferID;
                    tmp.changed =  tmpAct.changed;
                    lstrcpy( tmp.FilePath, currFile );
                    AddListData( &tmp );
                }
                break;
            }

            ActionDataLock.lock();
            ActionDataList.pop_front();
        }
        ActionDataLock.unlock();
        
        if ( ThreadNeedRefresh )
        {
            _LNhistory.refreshDlg();
            ThreadNeedRefresh = false;
        }

        Sleep( 100 );
    }

    return   0;
}

void DoSavedColor()
{
    if ( ByBookMark == MarkBookmark )
        return;

    for ( size_t i = 0; i < MarkHistory.size(); i++ )
    {
        if ( MarkHistory[i].BufferID == currTmpBufferID )
        {
            // ??Handle
            int MarkLine = ::SendMessage( curScintilla, SCI_MARKERLINEFROMHANDLE,
                                          MarkHistory[i].markHandle, 0 );
            MarkHistory[i].markHandle = ::SendMessage( curScintilla, SCI_MARKERADD,
                                        MarkLine, _SAVE_INDEX );
            // ??Marker
            ::SendMessage( curScintilla, SCI_MARKERDELETE, MarkLine, _MARK_INDEX );
            // ??
            //MarkHistory.erase(MarkHistory.begin()+i);
            //i--;
        }
    }

    // 2011-12-08 add for 0.4.7.4
    int MarkLine = -1;
    int markerMask1 = ( 1 << _MARK_INDEX );

    // ??marker???????,????
    while ( ( MarkLine = ::SendMessage( curScintilla, SCI_MARKERNEXT,
                                        MarkLine + 1, markerMask1 ) ) != -1 )
    {
        //TCHAR tmp[200]={0};
        //wsprintf(tmp,TEXT("%d"),MarkLine);
        //MessageBox(NULL,tmp,NULL,MB_OK);
        ::SendMessage( curScintilla, SCI_MARKERADD, MarkLine, _SAVE_INDEX );
    }

    ::SendMessage( curScintilla, SCI_MARKERDELETEALL, _MARK_INDEX, 0 );

}

int AddMarkFromLine( int line )
{
    int markHandle = -1;
    int state = ( int )::SendMessage( curScintilla, SCI_MARKERGET, line, 0 );

    if ( ByBookMark == MarkBookmark )
    {
        bool bookExist = ( _MARK_BOOKMARK & state ) != 0;

        if ( bookExist )
            return markHandle;

        //int backLine = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTLINE, 0, 0);
        //int pos = (int)::SendMessage(curScintilla, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage( curScintilla, SCI_GOTOLINE, line, 0 );
        ::SendMessage( nppData._nppHandle, NPPM_MENUCOMMAND, 0,
                       IDM_SEARCH_TOGGLE_BOOKMARK );
        markHandle = 0;
    }
    else
    {
        // 1<<_SAVE_INDEX ?????????
        if ( state == STATE_SAVE )
            ::SendMessage( curScintilla, SCI_MARKERDELETE, line, _SAVE_INDEX );
        else if ( state != 0 )
            return markHandle;

        markHandle = ::SendMessage( curScintilla, SCI_MARKERADD, line,
                                    _MARK_INDEX );
    }

    return markHandle;
}

void SetBookmark( int lineNo, int pos, int linesAdded, int len )
{

    // ??undo??,?Line?Position????,UNDO????,??curScintilla,Position?Line?????
    int handle = AddMarkFromLine( lineNo );

    if ( handle == -1 && linesAdded == 0 )
        return;

    if ( linesAdded > 0 )
    {
        for ( int line = 1; line <= linesAdded; line++ )
            AddMarkFromLine( lineNo + line );
    }

    // ???????  SCI_GETlineLenITION
    int lineLen = -1;

    if ( linesAdded == 0 )
    {
        lineLen = ( int )::SendMessage( curScintilla, SCI_LINELENGTH, lineNo,
                                        0 ) - len;
    }

    // ????
    MarkData tmp;
    tmp.position = pos;
    tmp.line = lineNo;
    tmp.lineAdd = linesAdded;
    tmp.BufferID = currTmpBufferID;
    tmp.markHandle = handle;
    tmp.lineLen = lineLen;
    MarkHistory.push_back( tmp );

}

bool RemoveMarkFromLine( int line )
{
    int state = ( int )::SendMessage( curScintilla, SCI_MARKERGET, line, 0 );

    if ( ByBookMark == MarkBookmark )
    {
        bool bookNotExist = ( _MARK_BOOKMARK & state ) == 0;

        if ( bookNotExist )
            return false;

        //int backLine = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTLINE, 0, 0);
        //int pos = (int)::SendMessage(curScintilla, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage( curScintilla, SCI_GOTOLINE, line, 0 );
        ::SendMessage( nppData._nppHandle, NPPM_MENUCOMMAND, 0,
                       IDM_SEARCH_TOGGLE_BOOKMARK );
    }
    else
    {
        // ????Mark??
        if ( state == 0 )
            return false;

        // 1<<_SAVE_INDEX
        if ( state == STATE_SAVE )
            ::SendMessage( curScintilla, SCI_MARKERDELETE, line, _SAVE_INDEX );
        else
            ::SendMessage( curScintilla, SCI_MARKERDELETE, line, _MARK_INDEX );
    }

    return true;
}

void DelBookmark( int lineNo, int pos, int lineAdd )
{
    if ( ByBookMark != MarkBookmark )
    {
        // ????????
        int canUndoFlag = ::SendMessage( curScintilla, SCI_CANUNDO, 0, 0 );

        if ( 0 == canUndoFlag )
        {
            ::SendMessage( curScintilla, SCI_MARKERDELETEALL, _MARK_INDEX, 0 );
            ::SendMessage( curScintilla, SCI_MARKERDELETEALL, _SAVE_INDEX, 0 );
            MarkHistory.clear();
            return;
        }
    }

    int isMarkedIndex = -1;
    int lastIndex = MarkHistory.size() - 1;
    int curSciCount = 0;
    int cur2Index[2] = { -1, -1};

    for ( int i = lastIndex; i > -1; i-- )
    {
        if ( MarkHistory[i].BufferID == currTmpBufferID )
        {
            curSciCount++;

            if ( curSciCount == 1 // ?????Scintilla????
               )
            {
                cur2Index[0] = i;//  SCI_GETlineLenITION

                if (  MarkHistory[i].line == lineNo
                        && MarkHistory[i].position == pos
                   )
                {
                    //   ????????,????????????
                    if ( lineAdd == 0 )
                    {
                        int lineLen = ( int )::SendMessage( curScintilla, SCI_LINELENGTH, lineNo,
                                                            0 );

                        if ( lineLen == MarkHistory[i].lineLen )
                            isMarkedIndex = i;
                    }
                    else
                        isMarkedIndex = i;
                }
            }

            if ( lineAdd == 0 )
                break;

            // ??????,???????????
            if ( curSciCount == 2 )
            {
                cur2Index[1] = i;
                break;
            }
        }
    }

    //?????????Mark
    for ( int i = 0; i < 2; i++ )
    {
        int tmpIndex = cur2Index[i];

        if ( -1 == tmpIndex )
            break;

        MarkData pretmp = MarkHistory[tmpIndex];
        int MarkLine = ::SendMessage( curScintilla, SCI_MARKERLINEFROMHANDLE,
                                      pretmp.markHandle, 0 );

        if ( MarkLine != pretmp.line )
        {
            //??
            RemoveMarkFromLine( MarkLine );
            MarkHistory[tmpIndex].markHandle = AddMarkFromLine( pretmp.line );
        }
    }

    if ( -1 == isMarkedIndex )
        // ?????????
        return;

    int lineB = lineNo;
    int lineE = lineNo + lineAdd;

    if ( lineAdd < 0 )
        lineE = lineNo - lineAdd;

    //bool hasRemoveMark = false;
    for ( int line = lineB; line <= lineE; line++ )
        RemoveMarkFromLine( line );

    MarkHistory.erase( MarkHistory.begin() + isMarkedIndex );
}

//////////  SELF FUNCTION END ////////
BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  reasonForCall,
                       LPVOID /*lpReserved*/ )
{
    switch ( reasonForCall )
    {
        case DLL_PROCESS_ATTACH:
            pluginInit( hModule );
            break;

        case DLL_PROCESS_DETACH:
            pluginCleanUp();
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}

extern "C" __declspec( dllexport ) void setInfo( NppData notpadPlusData )
{
    nppData = notpadPlusData;
    commandMenuInit();
}

extern "C" __declspec( dllexport ) const TCHAR *getName()
{
    return NPP_PLUGIN_NAME;
}

extern "C" __declspec( dllexport ) FuncItem *getFuncsArray( int *nbF )
{
    *nbF = nbFunc;
    return funcItem;
}
typedef struct _TBBUTTON
{
    int iBitmap;
    int idCommand;
    BYTE fsState;
    BYTE fsStyle;
#ifdef _WIN64
    BYTE bReserved[6];          // padding for alignment
#elif defined(_WIN32)
    BYTE bReserved[2];          // padding for alignment
#endif
    DWORD_PTR dwData;
    INT_PTR iString;
} TBBUTTON, NEAR *PTBBUTTON, *LPTBBUTTON;

typedef const TBBUTTON *LPCTBBUTTON;

#define TB_GETBUTTON      (WM_USER + 23)
#define TB_BUTTONCOUNT    (WM_USER + 24)
#define TB_COMMANDTOINDEX (WM_USER + 25)

static long preModifyPos = -1;
static long preModifyLineAdd = -1;

extern "C" __declspec( dllexport ) void beNotified( SCNotification
        *notifyCode )
{
    _LNhistory.setParent( nppData._nppHandle );
    int ModifyType = notifyCode->modificationType;

    if ( ( notifyCode->nmhdr.hwndFrom == nppData._nppHandle ) &&
            ( notifyCode->nmhdr.code == NPPN_TBMODIFICATION ) )
    {
        /* add toolbar icon */
        //g_TBPrevious.hToolbarIcon = ::LoadIcon(_LNhistory.getHinst(), MAKEINTRESOURCE(IDI_ICON_PREVIOUS));
        //(HBITMAP)::LoadImage(_LNhistory.getHinst(), MAKEINTRESOURCE(IDI_ICON_PREVIOUS), IMAGE_ICON, 16, 16, (LR_DEFAULTCOLOR));

        g_TBPreviousChg.hToolbarBmp = ( HBITMAP )::LoadImage( _LNhistory.getHinst(),
                                      MAKEINTRESOURCE( IDB_BITMAP3 ), IMAGE_BITMAP, 0, 0,
                                      ( LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS ) );
        ::SendMessage( nppData._nppHandle, NPPM_ADDTOOLBARICON,
                       ( WPARAM )funcItem[menuChgPrevious]._cmdID, ( LPARAM )&g_TBPreviousChg );

        g_TBPrevious.hToolbarBmp = ( HBITMAP )::LoadImage( _LNhistory.getHinst(),
                                   MAKEINTRESOURCE( IDB_BITMAP1 ), IMAGE_BITMAP, 0, 0,
                                   ( LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS ) );
        ::SendMessage( nppData._nppHandle, NPPM_ADDTOOLBARICON,
                       ( WPARAM )funcItem[menuPrevious]._cmdID, ( LPARAM )&g_TBPrevious );

        //g_TBNext.hToolbarIcon = ::LoadIcon(_LNhistory.getHinst(), MAKEINTRESOURCE(IDI_ICON_NEXT));
        g_TBNext.hToolbarBmp = ( HBITMAP )::LoadImage( _LNhistory.getHinst(),
                               MAKEINTRESOURCE( IDB_BITMAP2 ), IMAGE_BITMAP, 0, 0,
                               ( LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS ) );
        ::SendMessage( nppData._nppHandle, NPPM_ADDTOOLBARICON,
                       ( WPARAM )funcItem[menuNext]._cmdID, ( LPARAM )&g_TBNext );

        g_TBNextChg.hToolbarBmp = ( HBITMAP )::LoadImage( _LNhistory.getHinst(),
                                  MAKEINTRESOURCE( IDB_BITMAP4 ), IMAGE_BITMAP, 0, 0,
                                  ( LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS ) );
        ::SendMessage( nppData._nppHandle, NPPM_ADDTOOLBARICON,
                       ( WPARAM )funcItem[menuChgNext]._cmdID, ( LPARAM )&g_TBNextChg );

        /////?????/////////////////////////////////////////////////////////////////////
        for ( int i = 0; i < menuCount; i++ )
        {
            menuState[i] = true; // ???????
            IconID[i] = -1;
        }
    }

    switch ( notifyCode->nmhdr.code )
    {
        case NPPN_SHUTDOWN:
        {
            commandMenuCleanUp();
        }
        break;

        case NPPN_READY:
        {
            // ???? ReBarWindow32 ToolbarWindow32
            //nppData._nppHandle
            HWND hReBar = NULL;

            while ( true )
            {
                hReBar = FindWindowEx( nppData._nppHandle, hReBar, TEXT( "ReBarWindow32" ),
                                       NULL );

                if ( hReBar != NULL )
                {
                    hToolbar = FindWindowEx( hReBar, NULL, TEXT( "ToolbarWindow32" ), NULL );

                    if ( hToolbar != NULL )
                    {
                        //TB_SETIMAGELIST (LPARAM)&tButton
                        TBBUTTON tButton;

                        for ( int i = 0; i < menuCount; i++ )
                        {
                            // ??????ID???????
                            LRESULT lRetCode = SendMessage( hToolbar, TB_COMMANDTOINDEX,
                                                            ( WPARAM )funcItem[i]._cmdID, 0 );

                            if ( lRetCode != NULL )
                            {
                                //SendMessage(hToolbar,WM_COMMAND,0,(LPARAM)lRetCode);
                                // ??????????????ID?
                                ::SendMessage( hToolbar, TB_GETBUTTON, ( WPARAM )lRetCode,
                                               ( LPARAM ) &tButton );
                                IconID[i] = tButton.idCommand;
                            }
                        }

                        break;
                    }
                }
                else
                    break;
            }

            // ????????
            for ( int i = 0; i < menuCount; i++ )
            {
                if ( IconID[i] != -1 )
                {
                    ::SendMessage( hToolbar, TB_ENABLEBUTTON, ( WPARAM )IconID[i],
                                   MAKELONG( FALSE, 0 ) );
                    menuState[i] = false; // ??????
                }
            }

            // ????
            ::CheckMenuItem( ::GetMenu( nppData._nppHandle ),
                             funcItem[menuInCurr]._cmdID,
                             MF_BYCOMMAND | ( InCurr ? MF_CHECKED : MF_UNCHECKED ) );
            ::CheckMenuItem( ::GetMenu( nppData._nppHandle ),
                             funcItem[menuAutoRecord]._cmdID,
                             MF_BYCOMMAND | ( bAutoRecord ? MF_CHECKED : MF_UNCHECKED ) );
            ::EnableMenuItem( ::GetMenu( nppData._nppHandle ),
                              funcItem[menuManualRecord]._cmdID,
                              MF_BYCOMMAND | ( bAutoRecord ? MF_GRAYED : MF_ENABLED ) );

            ::CheckMenuItem( ::GetMenu( nppData._nppHandle ),
                             funcItem[menuNeedMark]._cmdID,
                             MF_BYCOMMAND | ( NeedMark ? MF_CHECKED : MF_UNCHECKED ) );

            ////////////////////////////////
            // ?? RecordContent ????
            if ( SaveRecord )
            {
                bool haveErrorInIni = false;
                TCHAR iniContent[RecordConentMax] = {0};
                ::GetPrivateProfileString( sectionName, strRecordContent, TEXT( "" ),
                                           iniContent, RecordConentMax, iniFilePath );
                TCHAR str[300] = {0};
                int len = sizeof( str );
                int j = 0;
                LocationInfo tmp;

                //::MessageBox(NULL,iniContent,TEXT(""),MB_OK);
                for ( int i = 0; i < RecordConentMax; i++ )
                {
                    if ( iniContent[i] == '\0' )
                        break;
                    else if ( iniContent[i] == '<' )
                    {
                        if ( str[0] == '1' )
                            tmp.changed = true;
                        else if ( str[0] == '0' )
                            tmp.changed = false;
                        else
                        {
                            // ????
                            haveErrorInIni = true;
                            break;
                        }

                        memset( str, 0, len );
                        j = 0;
                        continue;
                    }
                    else if ( iniContent[i] == '>' )
                    {
                        int pos = _tstoi( str );

                        if ( pos == 0 )
                        {
                            if (  str[0] == '0' && str[1] == '\0' )
                            {
                                // ??? 0 ??
                            }
                            else
                            {
                                // ????
                                haveErrorInIni = true;
                                break;
                            }
                        }

                        tmp.position = pos;
                        memset( str, 0, len );
                        j = 0;
                        continue;
                    }
                    else if ( iniContent[i] == '|' )
                    {
                        if ( str[0] == NEWFILE[0]
                                && str[1] == NEWFILE[1]
                                && str[2] == NEWFILE[2] )
                        {
                            // ?? new ??????
                        }
                        else if ( GetFileAttributes( str ) == 0xFFFFFFFF )
                        {
                            // ????????,?????????
                        }
                        else
                        {
                            lstrcpy( tmp.FilePath, str );
                            LocationList.push_back( tmp );
                        }

                        memset( str, 0, len );
                        j = 0;
                        continue;
                    }
                    else
                        str[j++] = iniContent[i];
                }

                if ( haveErrorInIni )
                    ClearLocationList();

                LocationPos = LocationList.size() - 1;
            }

            // ??????,????????
            DWORD   dwThreadId,   dwThrdParam   =   1;
            HANDLE   hThread;
            hThread =::CreateThread(
                         NULL, //   no   security   attributes
                         0,//   use   default   stack   size
                         ThreadFunc,//   thread   function
                         &dwThrdParam,//   argument   to   thread   function
                         0,//   use   default   creation   flags
                         &dwThreadId ); //   returns   the   thread   identifier
            CloseHandle( hThread );
            //InitBookmark();
            ready = true;
        }
        break;

        // mark setting remove to SCN_SAVEPOINTREACHED
        case NPPN_FILESAVED:
        {
            int which = -1;
            ::SendMessage( nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0,
                           ( LPARAM )&which );

            if ( which != -1 )
            {
                curScintilla = ( which == 0 ) ? nppData._scintillaMainHandle :
                               nppData._scintillaSecondHandle;
            }

            currTmpBufferID = notifyCode->nmhdr.idFrom;
            ::SendMessage( nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID,
                           ( WPARAM )notifyCode->nmhdr.idFrom, ( LPARAM )currTmpFile );
            lstrcpy( currFile, currTmpFile );
        }
        break;

        case NPPN_FILEBEFORECLOSE:
        {
            // ??????,???????????,?????????
            // ????????????????,???????????,????????,??????????,??????
            // ????,???? modifiers  4924 ??
            if (  !AllCloseFlag && notifyCode->modifiers == 4924 )
            {
                AllCloseFlag = true;

                if ( SaveRecord )
                    LocationSave.assign( LocationList.begin(), LocationList.end() );
            }
        }
        break;

        case NPPN_FILEBEFOREOPEN:
        case NPPN_FILEBEFORELOAD:
        {
            isOpenFile = true;
        }
        break;

        case NPPN_FILEOPENED:
        case NPPN_FILECLOSED:
        {
            isOpenFile = false;

            // ???????
            if ( AutoClean )
            {
                // ????????????????
                ActionData tmp;
                tmp.type = ActionClosed;
                ActionDataLock.lock();
                ActionDataList.push_back( tmp );
                ActionDataLock.unlock();

                // ????????????????????
                if ( notifyCode->nmhdr.code == NPPN_FILEOPENED && LocationList.size() > 0 )
                {
                    if ( LocationList[0].position == 0
                            && lstrcmp( LocationList[0].FilePath, NEWFILE ) == 0 )
                    {
                        TCHAR  **buffer;
                        long filecount2 = ::SendMessage( nppData._nppHandle, NPPM_GETNBOPENFILES, 0,
                                                         ( LPARAM )PRIMARY_VIEW );
                        buffer = new TCHAR*[filecount2];

                        for ( int i = 0; i < filecount2; i++ )
                            buffer[i] = new TCHAR[MAX_PATH];

                        ::SendMessage( nppData._nppHandle, NPPM_GETOPENFILENAMESPRIMARY,
                                       ( WPARAM )buffer, ( LPARAM )filecount2 );
                        // ?? new  1 ????????,?????
                        bool haveNew = false;

                        for ( int ifile = 0; ifile < filecount2; ifile++ )
                        {
                            if ( lstrcmp( buffer[ifile], NEWFILE ) == 0 )
                            {
                                haveNew = true;
                                break;
                            }
                        }

                        if ( !haveNew )
                            LocationList.pop_front();

                        for ( int i = 0; i < filecount2; i++ )
                            delete []buffer[i];

                        delete []buffer;
                    }
                }

                //LeaveCriticalSection(&criCounter);
            }
        }
        break;

        // ????

        case NPPN_BUFFERACTIVATED:
        {
            //scnNotification->nmhdr.code = NPPN_BUFFERACTIVATED;
            //scnNotification->nmhdr.hwndFrom = hwndNpp;
            //scnNotification->nmhdr.idFrom = activatedBufferID;
            //wsprintf(buffer,TEXT("%d,"),notifyCode->nmhdr.idFrom);
            // Get the current scintilla
            int which = -1;
            ::SendMessage( nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0,
                           ( LPARAM )&which );

            if ( which != -1 )
            {
                curScintilla = ( which == 0 ) ? nppData._scintillaMainHandle :
                               nppData._scintillaSecondHandle;
            }

            currTmpBufferID = notifyCode->nmhdr.idFrom;
            ::SendMessage( nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID,
                           ( WPARAM )notifyCode->nmhdr.idFrom, ( LPARAM )currTmpFile );
            ActionData tmp;
            tmp.type = ActionActive;
            ActionDataLock.lock();
            ActionDataList.push_back( tmp );
            ActionDataLock.unlock();

            // ????? currBufferID currFile
            if ( ready )
            {
                // ??????
                //bufferChanged = true;
                if ( !PositionSetting )
                {
                    PageActive = true;
                    PrePositionNumber = -1;
                    AddList();
                }
                else
                {
                    // ???????
                    PageActive = false;
                }
            }
        }
        break;

        case SCN_UPDATEUI:
        {
            if ( ready )
            {
                if ( notifyCode->nmhdr.hwndFrom != nppData._scintillaMainHandle &&
                        notifyCode->nmhdr.hwndFrom != nppData._scintillaSecondHandle )
                    break;

                AddList();
            }
        }
        break;

        case SCN_MODIFIED:
        {
            // ?????????????,?????????,??????
            //if(ModifyType == 0x11 || ModifyType == 0x12   // ??????????
            //|| ModifyType == 0xA1 || ModifyType == 0xA2 // ????
            //|| ModifyType == 0xC1 || ModifyType == 0xC2 // ????
            //)
            //{
            ////    break;
            //}
            // ??????
            //TCHAR tmp[100]={0};
            //wsprintf(tmp, TEXT("0x%X\n"),ModifyType);
            //OutputDebugString(tmp);
            if ( isOpenFile )
                break;

            if ( notifyCode->nmhdr.hwndFrom != nppData._scintillaMainHandle &&
                    notifyCode->nmhdr.hwndFrom != nppData._scintillaSecondHandle )
                break;

            if ( LocationList.size() == 0 )
            {
                // ????
                break;
            }

            long pos = 0; long len = 0;
            bool flag = false;

            // SC_MOD_BEFOREINSERT SC_MOD_INSERTTEXT SC_MOD_BEFOREDELETE SC_MOD_DELETETEXT
            if ( ModifyType & SC_MOD_INSERTTEXT )
            {
                pos = notifyCode->position;
                len = notifyCode->length;
                flag = true;
                //}else if( ModifyType &  ){
            }
            else if ( ModifyType & SC_MOD_DELETETEXT )
            {
                pos = notifyCode->position;
                len = -notifyCode->length;
                flag = true;
            }

            if ( flag )
            {
                if ( notifyCode->text == NULL && ( ModifyType == 0x12
                                                   || ModifyType == 0x11 ) )
                {
                    // ?????????,??????
                    break;
                }

                if ( 0 == pos )
                {
                    // maybe search in files, checked it
                    // Get the current scintilla
                    HWND tmpcurScintilla = ( HWND ) - 1;
                    int which = -1;
                    ::SendMessage( nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0,
                                   ( LPARAM )&which );

                    if ( which != -1 )
                    {
                        tmpcurScintilla = ( which == 0 ) ? nppData._scintillaMainHandle :
                                          nppData._scintillaSecondHandle;
                    }

                    if ( -1 == ( int )tmpcurScintilla )
                    {
                        // search in files
                        break;
                    }

                    //  SC_PERFORMED_USER| SC_MOD_DELETETEXT
                    // ???0x12?0x11????????????
                    // && (ModifyType == 0x12 || ModifyType == 0x11)
                    if ( notifyCode->text == NULL && 1024 == notifyCode->length )
                    {
                        // Open New file // ??????? || ModifyType == 0x2011
                        //isOpenFile = true;
                        break;
                    }
                }

                // ?????
                ActionData tmp;
                tmp.type = ActionModify;
                tmp.position = pos;
                tmp.length = len;
                ActionDataLock.lock();
                ActionDataList.push_back( tmp );
                ActionDataLock.unlock();

                if ( NeedMark )
                {
                    // ??????????????,?????????,??????Mark
                    isAutoModify = ( ::SendMessage( curScintilla, SCI_GETUNDOCOLLECTION, 0,
                                                    0 ) == 0 );

                    if ( !isAutoModify )
                    {
                        // ????????,??POS??line??
                        //int line = ::SendMessage(nppData._nppHandle,NPPM_GETCURRENTLINE, 0, 0);
                        // ????????,??????,????????
                        if ( ModifyType &  SC_PERFORMED_UNDO )
                        {
                            int line = -1;

                            // ???????????,????????0,??????????
                            if ( notifyCode->linesAdded == 0 && preModifyPos != pos
                                    && preModifyPos != -1 )
                            {
                                line = ::SendMessage( curScintilla, SCI_LINEFROMPOSITION, preModifyPos, 0 );
                                DelBookmark( line, preModifyPos, preModifyLineAdd );
                            }

                            // ?????????0,??????,???????
                            if ( notifyCode->linesAdded != 0 || ( ModifyType & SC_LASTSTEPINUNDOREDO ) )
                            {
                                line = ::SendMessage( curScintilla, SCI_LINEFROMPOSITION, pos, 0 );
                                DelBookmark( line, pos, notifyCode->linesAdded );
                                preModifyPos = -1;
                            }
                            else
                            {
                                preModifyPos = pos;
                                preModifyLineAdd = notifyCode->linesAdded;
                            }
                        }
                        else
                        {
                            // SC_PERFORMED_REDO
                            int line = ::SendMessage( curScintilla, SCI_LINEFROMPOSITION,
                                                      notifyCode->position, 0 );
                            SetBookmark( line, notifyCode->position, notifyCode->linesAdded, len );
                        }
                    }
                }
            }
        }
        break;

        case SCN_SAVEPOINTREACHED:
        {
            if ( NeedMark && !isOpenFile )
            {
                // ?????????Mark??
                DoSavedColor();
            }
        }
        break;

        default:
            return;
    }
}


// Here you can process the Npp Messages
// I will make the messages accessible little by little, according to the need of plugin development.
// Please let me know if you need to access to some messages :
// http://sourceforge.net/forum/forum.php?forum_id=482781
//

extern "C" __declspec( dllexport ) LRESULT messageProc( UINT /*Message*/,
        WPARAM /*wParam*/, LPARAM /*lParam*/ )
{
    // only support WM_SIZE and  WM_MOVE
    return TRUE;
}

#ifdef UNICODE
extern "C" __declspec( dllexport ) BOOL isUnicode()
{
    return TRUE;
}
#endif //UNICODE
