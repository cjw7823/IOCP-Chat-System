#include "IOCP_Server.h"
#include "ClientSession.h"

void Log(const std::string& str)
{
	static std::mutex g_logMutex;
	std::lock_guard<std::mutex> lock(g_logMutex);
	std::cout << str << std::endl;
}

bool IOCP_Server::Initialize()
{
	assert(mApp == nullptr);
	mApp = this;

	WSADATA wsaData = { 0 };
	if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandler(L"윈속을 초기화 할 수 없습니다.");

	::InitializeCriticalSection(&mCS);

	//Ctrl+C 이벤트 등록.
	if (::SetConsoleCtrlHandler(
		(PHANDLER_ROUTINE)CtrlHandler, TRUE) == FALSE)
		ErrorHandler(L"Ctrl+C 처리기를 등록할 수 없습니다.");

	/*
		1,2번 인자에 따라 두가지 동작을 보인다.
		-IOCP Queue를 만들며 IOCP 로직 구축.
		-IOCP Queue에 등록하여 관리.
	*/
	mhIOCP = ::CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,	//1. 연결된 파일 없음
		NULL,					//2. 기존 핸들 없음.
		0,						//식별자(Key) 해당되지 않음.
		0);						//스레드 개수는 os에 맡김

	if (mhIOCP == NULL)
		ErrorHandler(L"IOCP를 생성할 수 없습니다.");

	HANDLE hThread;
	DWORD dwThreadID;
	for (int i = 0; i < MAX_THREAD_CNT; i++)
	{
		hThread = ::CreateThread(NULL,	//보안속성 상속
			0,
			mApp->IOCP_WorkerThread,
			(LPVOID)NULL,
			0,
			&dwThreadID);

		::CloseHandle(hThread);
	}

	mListenSocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (mListenSocket == INVALID_SOCKET)
		ErrorHandler(L"리스닝 소켓을 생성할 수 없습니다.");

	SOCKADDR_IN addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = ::htonl(INADDR_ANY);
	addr.sin_port = ::htons(port);

	if (::bind(mListenSocket, (SOCKADDR*)&addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
		ErrorHandler(L"포트가 이미 사용중입니다.");

	if (::listen(mListenSocket, SOMAXCONN) == SOCKET_ERROR)
		ErrorHandler(L"리슨 상태로 전환할 수 없습니다.");

	hThread = ::CreateThread(NULL,
		0,
		mApp->IOCP_AcceptThread,
		NULL,
		0,
		&dwThreadID);
	::CloseHandle(hThread);

	mShutdownEvent = ::CreateEvent(
		nullptr,
		TRUE,
		FALSE,
		nullptr);

	return TRUE;
}

void IOCP_Server::Run()
{
	Log(u8"[채팅서버 시작]");

	::WaitForSingleObject(mShutdownEvent, INFINITE);

	ReleaseServer();
	Log(u8"[채팅서버 종료]");
}

void IOCP_Server::ReleaseServer()
{
	::EnterCriticalSection(&mCS);
	for (auto& it : mSessions)
	{
		::shutdown(it->hSocket, SD_BOTH);
		::closesocket(it->hSocket);
	}
	mSessions.clear();
	::LeaveCriticalSection(&mCS);

	if (mListenSocket != INVALID_SOCKET)
	{
		::closesocket(mListenSocket);
		mListenSocket = INVALID_SOCKET;
	}

	if (mhIOCP != NULL)
	{
		::CloseHandle(mhIOCP);
		mhIOCP = NULL;
	}

	::Sleep(500);
	::DeleteCriticalSection(&mCS);

	::WSACleanup();
}

void IOCP_Server::CloseSession(ClientSession* session)
{
	SOCKET& hSocket = session->hSocket;

	::shutdown(hSocket, SD_BOTH);
	::closesocket(hSocket);

	::EnterCriticalSection(&mCS);

	for (auto it = mSessions.begin(); it != mSessions.end(); it++)
	{
		if ((*it)->hSocket == hSocket)
		{
			//순서가 중요하지 않으므로 swap-and-pop
			*it = std::move(mSessions.back());
			mSessions.pop_back();
			break;
		}
	}

	::LeaveCriticalSection(&mCS);
}

BOOL IOCP_Server::CtrlHandler(DWORD dwType)
{
	if (dwType == CTRL_C_EVENT)
	{
		::SetEvent(mApp->mShutdownEvent);
		return TRUE;
	}

	return FALSE;
}

void IOCP_Server::SendMessageAll(const std::string& str, ClientSession* session)
{
	std::vector<ClientSession*>::iterator it;

	::EnterCriticalSection(&mCS);

	for (it = mSessions.begin(); it != mSessions.end(); it++)
	{
		if (session == *it) continue;

		IoContext* ctx = new IoContext(*it, IoType::Send);
		::ZeroMemory(&ctx->overlapped, sizeof(WSAOVERLAPPED));
		mApp->PostSend(*it, ctx, str.c_str(), str.size());
	}

	::LeaveCriticalSection(&mCS);
}

DWORD __stdcall IOCP_Server::IOCP_AcceptThread(LPVOID pParam)
{
	SOCKET clientSocket;
	SOCKADDR clientAddr;
	int nAddrSize = sizeof(SOCKADDR);

	while ((clientSocket = ::accept(mApp->mListenSocket, &clientAddr, &nAddrSize)) != INVALID_SOCKET)
	{
		Log(u8"새 클라이언트가 연결됐습니다.");

		auto pNewUser = new ClientSession();
		pNewUser->hSocket = clientSocket;

		::EnterCriticalSection(&mApp->mCS);
		mApp->mSessions.push_back(pNewUser);
		::LeaveCriticalSection(&mApp->mCS);

		IoContext* ctx = new IoContext(pNewUser, IoType::Recv);
		::ZeroMemory(&ctx->overlapped, sizeof(WSAOVERLAPPED));

		::CreateIoCompletionPort(
			(HANDLE)clientSocket,
			mApp->mhIOCP,
			(ULONG_PTR)pNewUser, //IOCP Queue에 들어갈 소켓들을 구분할 키.
			0);

		if (!mApp->PostRecv(pNewUser, ctx))
		{
			delete ctx;
			delete pNewUser;
			ErrorHandler(L"WSARecv 등록에 실패했습니다.");
		}
	}

	return 0;
}

DWORD __stdcall IOCP_Server::IOCP_WorkerThread(LPVOID pParam)
{
	Log(u8"[IOCP 작업자 스레드 시작]");

	ClientSession* pSession = nullptr;
	LPWSAOVERLAPPED	pWol = NULL;
	IoContext* ctx;
	DWORD dwTransferredSize = 0;

	while (TRUE)
	{
		bool bResult = ::GetQueuedCompletionStatus(
			mApp->mhIOCP,
			&dwTransferredSize,
			reinterpret_cast<PULONG_PTR>(&pSession),
			&pWol,
			INFINITE);
		
		if (pWol == NULL)
		{
			if (bResult)
				Log(u8"Worker 스레드 종료: 사용자 정의 completion packet");
			else //completion packet 자체를 dequeue 못함
				Log(u8"Worker 스레드 종료: IOCP 핸들 닫힘 / dequeue 실패 / Timeout(INFINITE가 아니라면)");

			break;
		}

		ctx = CONTAINING_RECORD(pWol, IoContext, overlapped);

		if (!bResult) //I/O는 완료됐지만 실패
		{
			Log(u8"Client Disconnect / 네트워크 오류 / send-recv 실패");
			mApp->CloseSession(pSession);
			delete ctx;
			continue;
		}

		/*
		overlapped 멤버의 위치를 기준으로 구조체 시작 주소를 역산.
		IoContext 메모리
		[ base 주소 ] ---------------------
		| overlapped |  ← pOverlapped
		| wsaBuf     |
		| buffer     |
		| type       |
		----------------------------------
		*/
		
		switch (ctx->mType)
		{
		case IoType::Recv:
			mApp->OnRecvCompleted(pSession, ctx, dwTransferredSize);
			break;
		case IoType::Send:
			mApp->OnSendCompleted(pSession, ctx, dwTransferredSize);
			break;
		default:
			break;
		}
	}

	Log(u8"[IOCP 작업자 스레드 종료]");
	return 0;
}

bool IOCP_Server::PostRecv(ClientSession* session, IoContext* ctx)
{
	DWORD dwReceiveSize = 0;
	DWORD dwFlag = 0;

	::ZeroMemory(&ctx->overlapped, sizeof(WSAOVERLAPPED));
	ctx->mType = IoType::Recv;
	ctx->wsaBuf.buf = ctx->buffer;
	ctx->wsaBuf.len = BUFFER_SIZE;

	int result = ::WSARecv(
		session->hSocket,
		&ctx->wsaBuf,
		1,
		&dwReceiveSize,
		&dwFlag,
		&ctx->overlapped,
		NULL);

	if (result == SOCKET_ERROR)
	{
		int err = ::WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			return false;
		}
	}
	return true;
}

bool IOCP_Server::PostSend(ClientSession* session, IoContext* ctx, const char* data, size_t len)
{
	if (len > BUFFER_SIZE)
		return false;

	::ZeroMemory(&ctx->overlapped, sizeof(WSAOVERLAPPED));
	ctx->mType = IoType::Send;

	memcpy(ctx->buffer, data, len);
	ctx->wsaBuf.buf = ctx->buffer;
	ctx->wsaBuf.len = static_cast<ULONG>(len);

	DWORD sentBytes = 0;
	int result = ::WSASend(
		session->hSocket,
		&ctx->wsaBuf,
		1,
		&sentBytes,	//의미없음.
		0,
		&ctx->overlapped,
		NULL);

	if (result == SOCKET_ERROR)
	{
		int err = ::WSAGetLastError();
		if (err != WSA_IO_PENDING)
			return false;
	}

	return true;
}

bool IOCP_Server::OnRecvCompleted(ClientSession* session, IoContext* ctx, DWORD bytes)
{
	// 1. 클라이언트가 소켓을 정상적으로 닫고 연결을 끊은 경우.
	if (bytes == 0)
	{
		mApp->CloseSession(session);
		delete ctx; // IoContext는 더 이상 필요 없으므로 해제
		Log(u8"클라이언트가 정상적으로 연결을 종료함.");
		return true;
	}

	// 2. 데이터 수신 처리: PacketHeader 기반으로 조립
	constexpr size_t HEADER_SIZE = sizeof(PacketHeader);
	const char* src = ctx->buffer;
	size_t srcOffset = 0;
	size_t remaining = static_cast<size_t>(bytes);

	auto& ab = session->assemblyBuffer;

	while (remaining > 0)
	{
		//헤더가 아직 완성되지 않은 경우 헤더 바이트를 먼저 채운다.
		if (ab.size() < HEADER_SIZE)
		{
			size_t need = HEADER_SIZE - ab.size();
			size_t take = (need < remaining) ? need : remaining;

			ab.insert(ab.end(), src + srcOffset, src + srcOffset + take);
			srcOffset += take;
			remaining -= take;

			//헤더가 아직 완성되지 않았으면 다음 WSARecv를 기다린다.
			if (ab.size() < HEADER_SIZE)
				break;
		}

		//헤더를 파싱하여 payloadSize 계산
		PacketHeader header;
		memcpy(&header, ab.data(), HEADER_SIZE);
		uint32_t payloadSize = ntohl(header.payloadSize); // 네트워크 바이트 순서 -> 호스트 바이트 순서

		{
			//안전 장치: 비정상적으로 큰 payloadSize를 걸러냄 (예: 10MB 제한)
			const uint32_t MAX_PAYLOAD = 10 * 1024 * 1024;
			if (payloadSize > MAX_PAYLOAD)
			{
				Log(u8"비정상적으로 큰 패킷을 감지하여 연결을 종료합니다.");
				mApp->CloseSession(session);
				delete ctx;
				return false;
			}
		}

		size_t totalNeeded = HEADER_SIZE + static_cast<size_t>(payloadSize);

		//payload가 모두 들어올 때까지 추가
		if (ab.size() < totalNeeded)
		{
			size_t need = totalNeeded - ab.size();
			size_t take = (need < remaining) ? need : remaining;

			ab.insert(ab.end(), src + srcOffset, src + srcOffset + take);
			srcOffset += take;
			remaining -= take;

			// 아직 전체 패킷이 완성되지 않았으면 다음 WSARecv를 기다린다.
			if (ab.size() < totalNeeded)
				break;
		}

		//전체 패킷이 완성
		if (ab.size() >= totalNeeded)
		{
			std::string payload(ab.data() + HEADER_SIZE, ab.data() + totalNeeded);
			mApp->SendMessageAll(payload, session);

			// 처리한 바이트(헤더+payload)를 assemblyBuffer에서 삭제
			ab.erase(ab.begin(), ab.begin() + totalNeeded);

			// 이어서 assemblyBuffer에 남아있는 데이터가 있으면 루프를 통해 다음 패킷도 처리
		}
	}

	// 수신 버퍼 초기화 및 다음 recv 등록
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
	if (!mApp->PostRecv(session, ctx))
	{
		delete ctx;
		ErrorHandler(L"WSARecv 등록에 실패했습니다.");
		return false;
	}

	return true;
}

bool IOCP_Server::OnSendCompleted(ClientSession* session, IoContext* ctx, DWORD bytes)
{
	//WSASend() 가 부분전송 되었을 가능성.

	ctx->transferredLen += bytes;
	if (ctx->transferredLen < ctx->totalLen)
	{
		::ZeroMemory(&ctx->overlapped, sizeof(WSAOVERLAPPED));
		ctx->wsaBuf.buf = ctx->buffer + ctx->transferredLen;
		ctx->wsaBuf.len = static_cast<ULONG>(ctx->totalLen - ctx->transferredLen);

		DWORD sentBytes = 0;
		int result = ::WSASend(
			session->hSocket,
			&ctx->wsaBuf,
			1,
			&sentBytes,
			0,
			&ctx->overlapped,
			NULL);

		if (result == SOCKET_ERROR)
		{
			int err = ::WSAGetLastError();
			if (err != WSA_IO_PENDING)
				return false;
		}
		return true;
	}

	delete ctx;
	return true;
}
