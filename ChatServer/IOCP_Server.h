#pragma once
#include "pch.h"

#define MAX_THREAD_CNT 4

#define ErrorHandler(msg)				\
{										\
	mApp->ReleaseServer();				\
	THROW_RUNTIME_ERROR(msg);			\
}

struct ClientSession;
struct IoContext;

class IOCP_Server
{
public:
	bool Initialize();
	void Run();
	void ReleaseServer();

private:
	void CloseSession(ClientSession* session);
	void SendMessageAll(const std::string& str, ClientSession* session);

	static BOOL WINAPI CtrlHandler(DWORD dwType);
	static DWORD WINAPI IOCP_AcceptThread(LPVOID pParam);
	static DWORD WINAPI IOCP_WorkerThread(LPVOID pParam);

	bool PostRecv(ClientSession* session);
	bool PostSend(ClientSession* session, IoContext* ctx);
	bool OnRecvCompleted(ClientSession* session, IoContext* ctx, DWORD bytes);
	bool OnSendCompleted(ClientSession* session, IoContext* ctx, DWORD bytes);

private:
	inline static IOCP_Server* mApp = nullptr;
	HANDLE mhIOCP = nullptr;
	HANDLE mShutdownEvent = nullptr;
	SOCKET mListenSocket = INVALID_SOCKET;

	CRITICAL_SECTION mCS;

	// 데이터 크기가 작으므로 중간 삭제가 잦더라도 캐쉬 친화적인 vector 사용.(list 대신.)
	std::vector<ClientSession*> mSessions;

	UINT port = 7575;
};