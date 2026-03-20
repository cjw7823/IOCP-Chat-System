#include "IOCP_Server.h"
#include "ClientSession.h"

#pragma comment(lib, "libsodium.lib")

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

	if (sodium_init() < 0)
		return false;

	if (!mDB.Open("chat.db"))
		return false;

	if (!mDB.CreateUserTable())
		return false;

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
	for (auto s : mSessions)
	{
		if (s->hSocket != INVALID_SOCKET)
		{
			::shutdown(s->hSocket, SD_BOTH);
			::closesocket(s->hSocket);
			s->hSocket = INVALID_SOCKET;
		}
		delete s;
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

	if (mShutdownEvent != NULL)
	{
		::CloseHandle(mShutdownEvent);
		mShutdownEvent = NULL;
	}

	//추후 다른 스레드들의 핸들로 join 후 종료로직.

	::Sleep(500);
	::DeleteCriticalSection(&mCS);

	::WSACleanup();
}

void IOCP_Server::CloseSession(ClientSession* session)
{
	SOCKET& hSocket = session->hSocket;

	{
		::EnterCriticalSection(&mCS);

		auto it = std::find(mSessions.begin(), mSessions.end(), session);
		if (it != mSessions.end())
			mSessions.erase(it);

		::LeaveCriticalSection(&mCS);
	}

	if (hSocket != INVALID_SOCKET)
	{
		::shutdown(hSocket, SD_BOTH);
		::closesocket(hSocket);
		hSocket = INVALID_SOCKET;

		delete session;
		session = nullptr;
	}
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
	std::vector<ClientSession*> targets;

	//락 최소화. 대상만 스냅샷.
	::EnterCriticalSection(&mCS);
	for (auto s : mSessions)
	{
		if (s != session)
			targets.push_back(s);
	}
	::LeaveCriticalSection(&mCS);

	const std::string& senderId = session->userID;

	const uint16_t senderIdLen = static_cast<uint16_t>(senderId.size());
	const uint16_t messageLen = static_cast<uint16_t>(str.size());

	ChatExtHeader chatExt{};
	chatExt.senderIdLen = htons(senderIdLen);
	chatExt.messageLen = htons(messageLen);

	const size_t headerSize = sizeof(PacketHeader);
	const size_t payloadSize = str.size();

	PacketHeader header{};
	header.cmd = htons(static_cast<uint16_t>(CMDCODE::ChatMessage));
	header.extHeaderSize = htons(static_cast<uint16_t>(sizeof(ChatExtHeader)));
	header.payloadSize = htonl(static_cast<uint32_t>(senderIdLen + messageLen)); //호스트->네트워크

	const size_t totalLen =
		sizeof(PacketHeader) +
		sizeof(ChatExtHeader) +
		senderIdLen +
		messageLen;

	if (totalLen > BUFFER_SIZE)
	{
		Log(u8"전송할 메세지 크기가 너무 큽니다.");
		ErrorHandler(L"전송할 메세지 크기가 너무 큽니다.");
	}

	for (auto& s : targets)
	{
		IoContext* ctx = new IoContext(s, IoType::Send);
		ctx->totalLen = totalLen;

		size_t offset = 0;

		memcpy(ctx->buffer + offset, &header, sizeof(PacketHeader));
		offset += sizeof(PacketHeader);

		memcpy(ctx->buffer + offset, &chatExt, sizeof(ChatExtHeader));
		offset += sizeof(ChatExtHeader);

		memcpy(ctx->buffer + offset, senderId.data(), senderIdLen);
		offset += senderIdLen;

		memcpy(ctx->buffer + offset, str.data(), messageLen);
		offset += messageLen;

		//중요. recv는 상관없지만(최대 버퍼만큼 받으므로) send는 얼마나 보낼지
		//Descriptor에 명시.
		ctx->wsaBuf.len = static_cast<ULONG>(ctx->totalLen);

		if (!PostSend(s, ctx))
		{
			delete ctx;
			// 필요 시 session 정리
		}
	}
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

		::CreateIoCompletionPort(
			(HANDLE)clientSocket,
			mApp->mhIOCP,
			(ULONG_PTR)pNewUser, //IOCP Queue에 들어갈 소켓들을 구분할 키.
			0);

		if (!mApp->PostRecv(pNewUser))
		{
			delete pNewUser;
			ErrorHandler(L"WSARecv 등록에 실패했습니다.");
		}

		::EnterCriticalSection(&mApp->mCS);
		mApp->mSessions.push_back(pNewUser);
		::LeaveCriticalSection(&mApp->mCS);
	}

	return 0;
}

DWORD __stdcall IOCP_Server::IOCP_WorkerThread(LPVOID pParam)
{
	Log(u8"[IOCP 작업자 스레드 시작]");

	ClientSession* pSession = nullptr;
	LPWSAOVERLAPPED	pWol = NULL;
	IoContext* ctx = nullptr;
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

		/*
		*	힙에서 찾기.
			overlapped 멤버의 위치를 기준으로 구조체 시작 주소를 역산.
			IoContext 메모리
			[ base 주소 ] ---------------------
			| overlapped |  ← pOverlapped
			| wsaBuf     |
			| buffer     |
			| type       |
			----------------------------------
		*/
		ctx = CONTAINING_RECORD(pWol, IoContext, overlapped);

		if (!bResult) //I/O는 완료됐지만 실패
		{
			Log(u8"Client Disconnect / 네트워크 오류 / send-recv 실패");
			mApp->CloseSession(pSession);
			delete ctx;
			continue;
		}

		switch (ctx->mType)
		{
		case IoType::Recv:
			if (!mApp->OnRecvCompleted(pSession, ctx, dwTransferredSize))
				mApp->CloseSession(pSession);
			break;

		case IoType::Send:
			if (!mApp->OnSendCompleted(pSession, ctx, dwTransferredSize))
				mApp->CloseSession(pSession);
			break;

		default:
			break;
		}
	}

	Log(u8"[IOCP 작업자 스레드 종료]");
	return 0;
}

bool IOCP_Server::PostRecv(ClientSession* session)
{
	IoContext* ctx = new IoContext(session, IoType::Recv);

	DWORD dwReceiveSize = 0;
	DWORD dwFlag = 0;

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
			delete ctx;
			return false;
		}
	}
	return true;
}

bool IOCP_Server::PostSend(ClientSession* session, IoContext* ctx)
{
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

bool IOCP_Server::OnRecvCompleted(ClientSession* session, IoContext* old_ctx, DWORD bytes)
{
	// 1. 클라이언트가 소켓을 정상적으로 닫고 연결을 끊은 경우.
	if (bytes == 0)
	{
		mApp->CloseSession(session);
		delete old_ctx; //IoContext는 더 이상 필요 없으므로 해제
		Log(u8"클라이언트가 정상적으로 연결을 종료함.");
		return true;
	}

	// 2. 데이터 수신 처리: PacketHeader 기반으로 조립
	constexpr size_t HEADER_SIZE = sizeof(PacketHeader);
	const char* src = old_ctx->buffer;
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
		PacketHeader netHeader;
		memcpy(&netHeader, ab.data(), HEADER_SIZE);

		PacketHeader hostHeader;
		hostHeader.cmd = ntohs(netHeader.cmd);
		hostHeader.extHeaderSize = ntohs(netHeader.extHeaderSize);
		hostHeader.payloadSize = ntohl(netHeader.payloadSize);

		const CMDCODE cmd = static_cast<CMDCODE>(hostHeader.cmd);
		const uint16_t extHeaderSize = hostHeader.extHeaderSize;
		const uint32_t payloadSize = hostHeader.payloadSize;

		{
			//안전 장치: 비정상적으로 큰 payloadSize를 걸러냄 (예: 10MB 제한)
			const uint32_t MAX_PAYLOAD = 10 * 1024 * 1024;
			if (payloadSize > MAX_PAYLOAD)
			{
				Log(u8"비정상적으로 큰 패킷을 감지하여 연결을 종료합니다.");
				delete old_ctx;
				return false;
			}
		}

		const size_t totalNeeded =
			HEADER_SIZE +
			static_cast<size_t>(extHeaderSize) +
			static_cast<size_t>(payloadSize);

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
			const char* packetBase = ab.data();
			const char* extHeaderPtr = packetBase + HEADER_SIZE;
			const char* payloadPtr = extHeaderPtr + extHeaderSize;
			std::string message(payloadPtr, payloadPtr + payloadSize);

			switch (cmd)
			{
			case CMDCODE::ChatMessage:
				// 클라이언트 -> 서버 채팅은 확장헤더 없이 메시지만 보낸다.
				if (extHeaderSize != 0)
				{
					Log(u8"클라이언트 채팅 패킷의 extHeaderSize가 0이 아님");
					delete old_ctx;
					return false;
				}

				if (!session->isLoggedIn)
				{
					Log(u8"로그인되지 않은 사용자의 채팅 요청");
					delete old_ctx;
					return false;
				}
				mApp->SendMessageAll(message, session);
				break;
			case CMDCODE::LoginRequest:
				mApp->HandleLoginRequest(message, session);
				break;
			case CMDCODE::RegisterRequest:
				mApp->HandleRegistRequest(message, session);
				break;
			default:
				Log(u8"CMDCODE Error");
				ErrorHandler(L"CMDCODE Error");
				delete old_ctx;
				return false;
			}

			ab.erase(ab.begin(), ab.begin() + totalNeeded);
		}
	}

	delete old_ctx;

	if (!mApp->PostRecv(session))
	{
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
	else //모두 전송되었으므로 ctx 삭제.
		delete ctx;

	return true;
}

bool IOCP_Server::HandleLoginRequest(const std::string& payload, ClientSession* session)
{
	if (payload.size() != sizeof(LoginRequestBody))
	{
		SendAuthResponse(session, CMDCODE::LoginResponse, AuthResult::ServerError);
		return false;
	}

	LoginRequestBody body{};
	memcpy(&body, payload.data(), sizeof(LoginRequestBody));

	std::string id(body.id);
	std::string pw(body.pw);

	if (id.empty() || pw.empty())
	{
		SendAuthResponse(session, CMDCODE::LoginResponse, AuthResult::InvalidId);
		return true;
	}

	std::string storedHash;
	bool found = mDB.GetUserAuthData(id, storedHash);

	if (!found)
	{
		SendAuthResponse(session, CMDCODE::LoginResponse, AuthResult::InvalidId);
		return true;
	}

	if (!VerifyPassword(storedHash, pw))
	{
		SendAuthResponse(session, CMDCODE::LoginResponse, AuthResult::WrongPassword);
		return true;
	}

	session->isLoggedIn = true;
	session->userID = id;

	SendAuthResponse(session, CMDCODE::LoginResponse, AuthResult::Success);
	return true;
}

bool IOCP_Server::HandleRegistRequest(const std::string& payload, ClientSession* session)
{
	if (payload.size() != sizeof(RegisterRequestBody))
	{
		SendAuthResponse(session, CMDCODE::RegisterResponse, AuthResult::ServerError);
		return false;
	}

	RegisterRequestBody body{};
	memcpy(&body, payload.data(), sizeof(RegisterRequestBody));

	std::string id(body.id);
	std::string pw(body.pw);

	if (id.empty() || pw.empty())
	{
		SendAuthResponse(session, CMDCODE::RegisterResponse, AuthResult::InvalidId);
		return true;
	}

	std::string hashedPW = MakePasswordHash(pw);
	if (hashedPW.empty())
	{
		SendAuthResponse(session, CMDCODE::RegisterResponse, AuthResult::ServerError);
		return false;
	}

	bool result = mDB.RegisterUser(id, hashedPW);

	if (!result)
	{
		SendAuthResponse(session, CMDCODE::RegisterResponse, AuthResult::DuplicateId);
		return true;
	}

	//session->isLoggedIn = true;
	//session->userID = id;

	SendAuthResponse(session, CMDCODE::RegisterResponse, AuthResult::Success);
	return true;
}

bool IOCP_Server::SendAuthResponse(ClientSession* session, CMDCODE cmd, AuthResult result)
{
	AuthResponseBody body{};
	body.result = htons(static_cast<uint16_t>(result));

	PacketHeader header{};
	header.cmd = htons(static_cast<uint16_t>(cmd));
	header.extHeaderSize = htons(0);
	header.payloadSize = htonl(sizeof(AuthResponseBody));

	IoContext* ctx = new IoContext(session, IoType::Send);
	ctx->totalLen = sizeof(PacketHeader) + sizeof(AuthResponseBody);

	memcpy(ctx->buffer, &header, sizeof(PacketHeader));
	memcpy(ctx->buffer + sizeof(PacketHeader), &body, sizeof(AuthResponseBody));

	ctx->wsaBuf.len = static_cast<ULONG>(ctx->totalLen);

	if (!PostSend(session, ctx))
	{
		delete ctx;
		return false;
	}

	return true;
}

std::string IOCP_Server::MakePasswordHash(const std::string& password)
{
	char hashed[crypto_pwhash_STRBYTES];
	ZeroMemory(&hashed, crypto_pwhash_STRBYTES);

	if (crypto_pwhash_str(
		hashed,
		password.c_str(),
		password.size(),
		crypto_pwhash_OPSLIMIT_INTERACTIVE,
		crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
		return "";

	return std::string(hashed);
}

bool IOCP_Server::VerifyPassword(const std::string& storedHash, const std::string& password)
{
	return crypto_pwhash_str_verify(
		storedHash.c_str(),
		password.c_str(),
		password.size()) == 0;
}
