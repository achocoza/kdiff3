/*
 * Copyright (c) 2003-2005, Sergey Zorin. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:

 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define _WIN32_WINNT 0x0502
#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_DEPRECATE

#include "server.h"

#include <stdio.h>

#include <windows.h>
#include <tchar.h>

#include <shlguid.h>
#include <olectl.h>
#include <objidl.h>

#include <objbase.h>
#include <initguid.h>

#include <KLocalizedString>
//#include <log/log.h>
//#include <log/log_message.h>
//#include <log/file_sink.h>
//#include <debug/trace.h>

#include "class_factory.h"

#define DllExport   __declspec( dllexport )

// registry key util struct
struct REGSTRUCT {
  LPCTSTR subkey;
  LPCTSTR name;
  LPCTSTR value;
};

SERVER* SERVER::_instance = 0;
static HINSTANCE server_instance; // Handle to this DLL itself.

//DEFINE_GUID(CLSID_DIFF_EXT, 0xA0482097, 0xC69D, 0x4DEC, 0x8A, 0xB6, 0xD3, 0xA2, 0x59, 0xAC, 0xC1, 0x51);
// New class id for DIFF_EXT for KDiff3
#ifdef _WIN64
// {34471FFB-4002-438b-8952-E4588D0C0FE9}
DEFINE_GUID( CLSID_DIFF_EXT, 0x34471FFB, 0x4002, 0x438b, 0x89, 0x52, 0xE4, 0x58, 0x8D, 0x0C, 0x0F, 0xE9 );
#else
DEFINE_GUID( CLSID_DIFF_EXT, 0x9f8528e4, 0xab20, 0x456e, 0x84, 0xe5, 0x3c, 0xe6, 0x9d, 0x87, 0x20, 0xf3 );
#endif

tstring SERVER::getRegistryKeyString( const tstring& subKey, const tstring& value )
{
   tstring keyName = m_registryBaseName;
   if (!subKey.empty())
      keyName += TEXT("\\")+subKey;

   HKEY key;
   HKEY baseKey = HKEY_CURRENT_USER;
   tstring result;
   for(;;)
   {
      if( RegOpenKeyEx( baseKey, keyName.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key ) == ERROR_SUCCESS )
      {
         DWORD neededSizeInBytes = 0;
         if (RegQueryValueEx(key, value.c_str(), 0, 0, 0, &neededSizeInBytes) == ERROR_SUCCESS)
         {
            DWORD length = neededSizeInBytes / sizeof( TCHAR );
            result.resize( length );
            if ( RegQueryValueEx( key, value.c_str(), 0, 0, (LPBYTE)&result[0], &neededSizeInBytes ) == ERROR_SUCCESS)
            {
               //Everything is ok, but we want to cut off the terminating 0-character
               result.resize( length - 1 );
               RegCloseKey(key);
               return result;
            }
            else
            {
               result.resize(0);
            }
         }

         RegCloseKey(key);
      }
      if (baseKey==HKEY_LOCAL_MACHINE)
         break;
      baseKey = HKEY_LOCAL_MACHINE;
   }

   // Error
   {
      LPTSTR message;
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0,
         GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &message, 0, 0);
      ERRORLOG( (tstring(TEXT("RegOpenKeyEx: ")+keyName+TEXT("->")+value) + TEXT(": ")) + message );                                        \
      LocalFree(message);
   }
   return result;
}


STDAPI
DllCanUnloadNow(void) {
  HRESULT ret = S_FALSE;

  if(SERVER::instance()->reference_count() == 0) {
    ret = S_OK;
  }

  return ret;
}

extern "C" int APIENTRY
DllMain(HINSTANCE instance, DWORD reason, LPVOID /* reserved */) {
//  char str[1024];
//  char* reason_string[] = {"DLL_PROCESS_DETACH", "DLL_PROCESS_ATTACH", "DLL_THREAD_ATTACH", "DLL_THREAD_DETACH"};
//  sprintf(str, "instance: %x; reason: '%s'", instance, reason_string[reason]);
//  MessageBox(0, str, TEXT("Info"), MB_OK);
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      server_instance = instance;
      SERVER::instance()->save_history();
      MESSAGELOG(TEXT("DLL_PROCESS_ATTACH"));
      break;

    case DLL_PROCESS_DETACH:
      MESSAGELOG(TEXT("DLL_PROCESS_DETACH"));
      SERVER::instance()->save_history();
      break;
  }

  return 1;
}

STDAPI
DllGetClassObject(REFCLSID rclsid, REFIID riid, void** class_object) {
  HRESULT ret = CLASS_E_CLASSNOTAVAILABLE;
  *class_object = 0;

  if (IsEqualIID(rclsid, CLSID_DIFF_EXT)) {
    CLASS_FACTORY* pcf = new CLASS_FACTORY();

    ret = pcf->QueryInterface(riid, class_object);
  }

  return ret;
}

/*extern "C" HRESULT STDAPICALLTYPE*/  STDAPI
DllRegisterServer() {
  return SERVER::instance()->do_register();
}

STDAPI
DllUnregisterServer() {
  return SERVER::instance()->do_unregister();
}

SERVER* SERVER::instance()
{
   if(_instance == 0)
   {
      _instance = new SERVER();
      _instance->initLogging();
      MESSAGELOG(TEXT("New Server instance"));
   }

   return _instance;
}

SERVER::SERVER()  : _reference_count(0)
{
   m_registryBaseName = TEXT("Software\\KDiff3\\diff-ext");
   m_pRecentFiles = 0;
   m_pLogFile = 0;
}

void SERVER::initLogging()
{
   tstring logFileName = getRegistryKeyString( TEXT(""), TEXT("LogFile") );
   if ( !logFileName.empty() )
   {
      m_pLogFile = _tfopen( logFileName.c_str(), TEXT("a+, ccs=UTF-8") );
      if (m_pLogFile)
      {
         _ftprintf( m_pLogFile, TEXT("\nSERVER::SERVER()\n") );
      }
   }
}

SERVER::~SERVER()
{
   if ( m_pLogFile )
   {
      _ftprintf( m_pLogFile, TEXT("SERVER::~SERVER()\n\n") );
      fclose( m_pLogFile );
   }

   delete m_pRecentFiles;
}

HINSTANCE
SERVER::handle() const
{
   return server_instance;
}

void
SERVER::lock() {
  InterlockedIncrement(&_reference_count);
}

void
SERVER::release() {
  InterlockedDecrement(&_reference_count);

  //if(InterlockedDecrement((LPLONG)&_reference_count) == 0)
  //   delete this;
}

void SERVER::logMessage( const char* function, const char* file, int line, const tstring& msg )
{
   SERVER* pServer = SERVER::instance();
   if ( pServer && pServer->m_pLogFile )
   {
      SYSTEMTIME st;
      GetSystemTime( &st );
      _ftprintf( pServer->m_pLogFile, TEXT("%04d/%02d/%02d %02d:%02d:%02d ")
#ifdef UNICODE
         TEXT("%S (%S:%d) %s\n"), // integrate char-string into wchar_t string
#else
         TEXT("%s (%s:%d) %s\n"),
#endif
         st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, function, file, line, msg.c_str() );
      fflush(pServer->m_pLogFile);
   }
}

std::list<tstring>&
SERVER::recent_files()
{
   LOG();
   if ( m_pRecentFiles==0 )
   {
      m_pRecentFiles = new std::list<tstring>;
   }
   else
   {
      m_pRecentFiles->clear();
   }
   MESSAGELOG(TEXT("Reading history from registry..."));
   for( int i=0; i<32; ++i )  // Max history size
   {
      TCHAR numAsString[10];
      _sntprintf( numAsString, 10, TEXT("%d"), i );
      tstring historyItem = getRegistryKeyString( TEXT("history"), numAsString );
      if ( ! historyItem.empty() )
         m_pRecentFiles->push_back( historyItem );
   }
   return *m_pRecentFiles;
}

void
SERVER::save_history() const
{
   if( m_pRecentFiles )
   {
      HKEY key;
      if( RegCreateKeyEx(HKEY_CURRENT_USER, (m_registryBaseName + TEXT("\\history")).c_str(), 0, 0,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, 0, &key, 0) == ERROR_SUCCESS )
      {
         LOG();
         //DWORD len = MAX_PATH;
         int n = 0;

         std::list<tstring>::const_iterator i;

         for(i = m_pRecentFiles->begin(); i!=m_pRecentFiles->end(); ++i, ++n )
         {
            tstring str = *i;
            TCHAR numAsString[10];
            _sntprintf( numAsString, 10, TEXT("%d"), n );
            if(RegSetValueEx(key, numAsString, 0, REG_SZ, (const BYTE*)str.c_str(), (DWORD)(str.size()+1)*sizeof(TCHAR) ) != ERROR_SUCCESS)
            {
               LPTSTR message;
               FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0,
                  GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                  (LPTSTR) &message, 0, 0);
               MessageBox(0, message, TEXT("KDiff3-diff-ext: Save history failed"), MB_OK | MB_ICONINFORMATION);
               LocalFree(message);
            }
         }
         for(; n<32; ++n )
         {
            TCHAR numAsString[10];
            _sntprintf( numAsString, 10, TEXT("%d"), n );
            RegDeleteValue(key, numAsString );
         }

         RegCloseKey(key);
      }
      else
      {
         SYSERRORLOG(TEXT("RegOpenKeyEx"));
      }
   }
}

HRESULT
SERVER::do_register() {
   LOG();
  TCHAR   class_id[MAX_PATH];
  LPWSTR  tmp_guid;
  HRESULT ret = SELFREG_E_CLASS;

  if (StringFromIID(CLSID_DIFF_EXT, &tmp_guid) == S_OK) {
#ifdef UNICODE
    _tcsncpy(class_id, tmp_guid, MAX_PATH);
#else
    wcstombs(class_id, tmp_guid, MAX_PATH);
#endif
    CoTaskMemFree((void*)tmp_guid);

    TCHAR    subkey[MAX_PATH];
    TCHAR    server_path[MAX_PATH];
    HKEY     key;
    LRESULT  result = NOERROR;
    DWORD    dwDisp;

    GetModuleFileName(SERVER::instance()->handle(), server_path, MAX_PATH);

    REGSTRUCT entry[] = {
      {TEXT("Software\\Classes\\CLSID\\%s"), 0, TEXT("diff-ext-for-kdiff3")},
      {TEXT("Software\\Classes\\CLSID\\%s\\InProcServer32"), 0, TEXT("%s")},
      {TEXT("Software\\Classes\\CLSID\\%s\\InProcServer32"), TEXT("ThreadingModel"), TEXT("Apartment")}
    };

    for(unsigned int i = 0; (i < sizeof(entry)/sizeof(entry[0])) && (result == NOERROR); i++) {
      _sntprintf(subkey, MAX_PATH, entry[i].subkey, class_id);
      result = RegCreateKeyEx(HKEY_CURRENT_USER, subkey, 0, 0, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &key, &dwDisp);

      if(result == NOERROR) {
        TCHAR szData[MAX_PATH];

        _sntprintf(szData, MAX_PATH, entry[i].value, server_path);
        szData[MAX_PATH-1]=0;

        result = RegSetValueEx(key, entry[i].name, 0, REG_SZ, (LPBYTE)szData, DWORD(_tcslen(szData)*sizeof(TCHAR)));
      }

      RegCloseKey(key);
    }

    if(result == NOERROR) {
      result = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Classes\\*\\shellex\\ContextMenuHandlers\\diff-ext-for-kdiff3"), 0, 0, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &key, &dwDisp);

      if(result == NOERROR) {

        result = RegSetValueEx(key, 0, 0, REG_SZ, (LPBYTE)class_id, DWORD(_tcslen(class_id)*sizeof(TCHAR)));

        RegCloseKey(key);

        //If running on NT, register the extension as approved.
        OSVERSIONINFO  osvi;

        osvi.dwOSVersionInfoSize = sizeof(osvi);
        GetVersionEx(&osvi);

        // NT needs to have shell extensions "approved".
        if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
          result = RegCreateKeyEx(HKEY_CURRENT_USER,
             TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"),
             0, 0, REG_OPTION_NON_VOLATILE, KEY_WRITE, 0, &key, &dwDisp);

          if(result == NOERROR) {
            TCHAR szData[MAX_PATH];

            lstrcpy(szData, TEXT("diff-ext"));

            result = RegSetValueEx(key, class_id, 0, REG_SZ, (LPBYTE)szData, DWORD(_tcslen(szData)*sizeof(TCHAR)));

            RegCloseKey(key);

            ret = S_OK;
          } else if (result == ERROR_ACCESS_DENIED) {
	    TCHAR msg[] = TEXT("Warning! You have unsufficient rights to write to a specific registry key.\n")
			  TEXT("The application may work anyway, but it is advised to register this module ")
			  TEXT("again while having administrator rights.");

	    MessageBox(0, msg, TEXT("Warning"), MB_ICONEXCLAMATION);

	    ret = S_OK;
          }
        }
        else {
          ret = S_OK;
        }
      }
    }
  }

  return ret;
}

HRESULT
SERVER::do_unregister() {
   LOG();
  TCHAR class_id[MAX_PATH];
  LPWSTR tmp_guid;
  HRESULT ret = SELFREG_E_CLASS;

  if (StringFromIID(CLSID_DIFF_EXT, &tmp_guid) == S_OK) {
#ifdef UNICODE
    _tcsncpy(class_id, tmp_guid, MAX_PATH);
#else
    wcstombs(class_id, tmp_guid, MAX_PATH);
#endif
    CoTaskMemFree((void*)tmp_guid);

    LRESULT result = NOERROR;
    TCHAR subkey[MAX_PATH];

    REGSTRUCT entry[] = {
      {TEXT("Software\\Classes\\CLSID\\%s\\InProcServer32"), 0, 0},
      {TEXT("Software\\Classes\\CLSID\\%s"), 0, 0}
    };

    for(unsigned int i = 0; (i < sizeof(entry)/sizeof(entry[0])) && (result == NOERROR); i++) {
      _stprintf(subkey, entry[i].subkey, class_id);
      result = RegDeleteKey(HKEY_CURRENT_USER, subkey);
    }

    if(result == NOERROR) {
      result = RegDeleteKey(HKEY_CURRENT_USER, TEXT("Software\\Classes\\*\\shellex\\ContextMenuHandlers\\diff-ext-for-kdiff3"));

      if(result == NOERROR) {
        //If running on NT, register the extension as approved.
        OSVERSIONINFO  osvi;

        osvi.dwOSVersionInfoSize = sizeof(osvi);
        GetVersionEx(&osvi);

        // NT needs to have shell extensions "approved".
        if(osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
          HKEY key;

          RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"), 0, KEY_ALL_ACCESS, &key);

          result = RegDeleteValue(key, class_id);

          RegCloseKey(key);

          if(result == ERROR_SUCCESS) {
            ret = S_OK;
          }
        }
        else {
          ret = S_OK;
        }
      }
    }
  }

  return ret;
}
