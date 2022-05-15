#include "pch.h"
#include <string>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <stdio.h>
#include "detours/detours.h"
#include "include/cef_v8.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/internal/cef_export.h"
#include "include/capi/cef_v8_capi.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <ctime>
#include "createfiles.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AheadLib
#include <Windows.h>
#include <thread>

#pragma comment(linker, "/EXPORT:vSetDdrawflag=_AheadLib_vSetDdrawflag,@1")
#pragma comment(linker, "/EXPORT:AlphaBlend=_AheadLib_AlphaBlend,@2")
#pragma comment(linker, "/EXPORT:DllInitialize=_AheadLib_DllInitialize,@3")
#pragma comment(linker, "/EXPORT:GradientFill=_AheadLib_GradientFill,@4")
#pragma comment(linker, "/EXPORT:TransparentBlt=_AheadLib_TransparentBlt,@5")

#define EXTERNC extern "C"
#define NAKED __declspec(naked)
#define EXPORT __declspec(dllexport)

#define ALCPP EXPORT NAKED
#define ALSTD EXTERNC EXPORT NAKED void __stdcall
#define ALCFAST EXTERNC EXPORT NAKED void __fastcall
#define ALCDECL EXTERNC NAKED void __cdecl
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



// defination of cef_v8context_get_current_context
typedef cef_v8context_t* (*get_context)();

PVOID v8getcontext_source = NULL;

std::string script;
_cef_frame_t* frame;
cef_v8context_t* contextl;
using namespace std;
void executeJavaScript(string script,string url="libeasycef/injext.js") {
	CefString exec_script = script;
	CefString purl = url;
	frame->execute_java_script(frame, exec_script.GetStruct(),purl.GetStruct(), 0);
}

bool injectsucceeded = false;

cef_v8context_t* v8getcontext_hooked() {
	cef_v8context_t* context = ((get_context)v8getcontext_source)();

	if (!injectsucceeded) {
		cef_browser_t* browser = context->get_browser(context);
		contextl = context;
		frame = browser->get_main_frame(browser);
		executeJavaScript(script);
	}
    
    return context;
}

BOOL APIENTRY InstallHook()
{
	createBetterNCMFiles();
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    v8getcontext_source = DetourFindFunction("libcef.dll", "cef_v8context_get_current_context");
    if (v8getcontext_source) {
        DetourAttach(&v8getcontext_source, v8getcontext_hooked);
    }
    LONG ret = DetourTransactionCommit();
    return ret == NO_ERROR;
}


BOOL APIENTRY UnInstallHook()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&v8getcontext_source, v8getcontext_hooked);
    LONG ret = DetourTransactionCommit();
    return ret == NO_ERROR;
}

namespace AheadLib
{
	HMODULE m_hModule = NULL;	// ԭʼģ����
	DWORD m_dwReturn[5] = { 0 };	// ԭʼ�������ص�ַ


	// ����ԭʼģ��
	inline BOOL WINAPI Load()
	{
		TCHAR tzPath[MAX_PATH];
		TCHAR tzTemp[MAX_PATH * 2];

		GetSystemDirectory(tzPath, MAX_PATH);
		lstrcat(tzPath, TEXT("\\msimg32"));
		m_hModule = LoadLibrary(tzPath);
		if (m_hModule == NULL)
		{
			wsprintf(tzTemp, TEXT("�޷����� %s�������޷��������С�"), tzPath);
			MessageBox(NULL, tzTemp, TEXT("AheadLib"), MB_ICONSTOP);
		}

		return (m_hModule != NULL);
	}

	// �ͷ�ԭʼģ��
	inline VOID WINAPI Free()
	{
		if (m_hModule)
		{
			FreeLibrary(m_hModule);
		}
	}

	// ��ȡԭʼ������ַ
	FARPROC WINAPI GetAddress(PCSTR pszProcName)
	{
		FARPROC fpAddress;
		CHAR szProcName[16];
		TCHAR tzTemp[MAX_PATH];

		fpAddress = GetProcAddress(m_hModule, pszProcName);
		if (fpAddress == NULL)
		{
			if (HIWORD(pszProcName) == 0)
			{
				//printf(szProcName, "%d", pszProcName);
				pszProcName = szProcName;
			}

			wsprintf(tzTemp, TEXT("�޷��ҵ����� %hs�������޷��������С�"), pszProcName);
			MessageBox(NULL, tzTemp, TEXT("AheadLib"), MB_ICONSTOP);
			ExitProcess(-2);
		}

		return fpAddress;
	}
}
using namespace AheadLib;
thread* servert;

string read_to_string(string path) {
	std::ifstream t(path);
	std::stringstream buffer;
	buffer << t.rdbuf();
	return string(buffer.str());
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, PVOID pvReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
		servert=new thread([=] {
			string datapath = string(getenv("USERPROFILE"))+"\\betterncm";
			httplib::Server svr;
			svr.Get("/addons", [&](const httplib::Request&, httplib::Response& res) {
				res.set_content(read_to_string(datapath+"/addons.json"), "application/json");
			});

			svr.Get("/injectsucceeded", [&](const httplib::Request& req, httplib::Response& res) {
				injectsucceeded = true;
			});

			svr.Get("/read_file", [&](const httplib::Request& req, httplib::Response& res) {
				string filename = req.get_param_value("filename");
				if (filename.find_first_of("..") != string::npos) {
					res.status = 502;
					res.set_content("Bad request","");
				}
				res.set_content(read_to_string(datapath + "/" + filename), "application/json");
			}); 

			svr.Post("/write_file", [&](const httplib::Request& req, httplib::Response& res) {
				ofstream file;
				string filename = req.get_param_value("filename");
				if (filename.find_first_of("..") != string::npos) {
					res.status = 502;
					res.set_content("Bad request", "");
				}
				file.open(datapath + "/" + filename);
				file << req.body;
				file.close();
				res.set_content(read_to_string(datapath + "/" + req.get_param_value("filename")), "application/json");
			});
			
			svr.Get("/explorer", [&](const httplib::Request& req, httplib::Response& res) {
				auto command = string("cmd /c explorer.exe \"") + datapath + "\"";
				system(command.c_str());
				res.set_content(command,"");
			});
			svr.listen("0.0.0.0", 3248);
		});

		script = R"(
if(!window["__BETTERNCM___"]){
window["__BETTERNCM___"]=true

var addons;
var reloadInject = {}
function main() {
    fetch("http://localhost:3248/addons").then(function (resp) { return resp.text() }).then(function (resp1) {
        addons = JSON.parse(resp1);
        setInterval(function () {
            for (var x = 0; x < addons.length; x++) {
                var addon = addons[x];
                if(addon.enabled)
                if (addon.devMode) {
                    fetch("http://localhost:3248/read_file?filename=" + addon.file)
                        .then(function (resp_2856) { return resp_2856.text() })
                        .then(function (resp2) {
                            if (reloadInject[addon.file] && resp2 != reloadInject[addon.file]) {
                                document.location.reload()
                            }
                        })
                }
            }
        }, 1000);
        for (var x = 0; x < addons.length; x++) {
            var addon = addons[x];
            if(addon.enabled)
            fetch("http://localhost:3248/read_file?filename=" + addon.file)
                .then(function (resp) { return resp.text() })
                .then(function (resp) {
                    reloadInject[addon.file] = resp
                    var script = document.createElement("script");
                    script.innerHTML = resp;
                    document.body.appendChild(script);
                })
		
        }
fetch("http://localhost:3248/injectsucceeded")
    });
}
setTimeout(function(){main()},1000)
}
)";
        InstallHook();
		DisableThreadLibraryCalls(hModule);

		for (INT i = 0; i < sizeof(m_dwReturn) / sizeof(DWORD); i++)
		{
			m_dwReturn[i] = TlsAlloc();
		}

		return Load();
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        
		for (INT i = 0; i < sizeof(m_dwReturn) / sizeof(DWORD); i++)
		{
			TlsFree(m_dwReturn[i]);
		}

		Free();
		UnInstallHook();
    }

    return TRUE;
}

ALCDECL AheadLib_vSetDdrawflag(void)
{
	// ���淵�ص�ַ�� TLS
	__asm PUSH m_dwReturn[0 * TYPE long];
	__asm CALL DWORD PTR[TlsSetValue];

	// ����ԭʼ����
	GetAddress("vSetDdrawflag")();

	// ��ȡ���ص�ַ������
	__asm PUSH EAX;
	__asm PUSH m_dwReturn[0 * TYPE long];
	__asm CALL DWORD PTR[TlsGetValue];
	__asm XCHG EAX, [ESP];
	__asm RET;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ��������
ALCDECL AheadLib_AlphaBlend(void)
{
	// ���淵�ص�ַ�� TLS
	__asm PUSH m_dwReturn[1 * TYPE long];
	__asm CALL DWORD PTR[TlsSetValue];

	// ����ԭʼ����
	GetAddress("AlphaBlend")();

	// ��ȡ���ص�ַ������
	__asm PUSH EAX;
	__asm PUSH m_dwReturn[1 * TYPE long];
	__asm CALL DWORD PTR[TlsGetValue];
	__asm XCHG EAX, [ESP];
	__asm RET;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ��������
ALCDECL AheadLib_DllInitialize(void)
{
	// ���淵�ص�ַ�� TLS
	__asm PUSH m_dwReturn[2 * TYPE long];
	__asm CALL DWORD PTR[TlsSetValue];

	// ����ԭʼ����
	GetAddress("DllInitialize")();

	// ��ȡ���ص�ַ������
	__asm PUSH EAX;
	__asm PUSH m_dwReturn[2 * TYPE long];
	__asm CALL DWORD PTR[TlsGetValue];
	__asm XCHG EAX, [ESP];
	__asm RET;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ��������
ALCDECL AheadLib_GradientFill(void)
{
	// ���淵�ص�ַ�� TLS
	__asm PUSH m_dwReturn[3 * TYPE long];
	__asm CALL DWORD PTR[TlsSetValue];

	// ����ԭʼ����
	GetAddress("GradientFill")();

	// ��ȡ���ص�ַ������
	__asm PUSH EAX;
	__asm PUSH m_dwReturn[3 * TYPE long];
	__asm CALL DWORD PTR[TlsGetValue];
	__asm XCHG EAX, [ESP];
	__asm RET;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ��������
ALCDECL AheadLib_TransparentBlt(void)
{
	// ���淵�ص�ַ�� TLS
	__asm PUSH m_dwReturn[4 * TYPE long];
	__asm CALL DWORD PTR[TlsSetValue];

	// ����ԭʼ����
	GetAddress("TransparentBlt")();

	// ��ȡ���ص�ַ������
	__asm PUSH EAX;
	__asm PUSH m_dwReturn[4 * TYPE long];
	__asm CALL DWORD PTR[TlsGetValue];
	__asm XCHG EAX, [ESP];
	__asm RET;
}