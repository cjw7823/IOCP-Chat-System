#include "pch.h"
#include "IOCP_Server.h"

#pragma comment(lib, "ws2_32")

int main()
{
    SetConsoleOutputCP(CP_UTF8); //콘솔창 인코딩 변환
    SetConsoleCP(CP_UTF8);       //input
    
    try
    {
        IOCP_Server app;
        if (!app.Initialize())
            THROW_RUNTIME_ERROR(L"Initial Error");

        app.Run();
    }
    catch (const std::exception& e)
    {
        std::wcout << e.what();
    }

    return 0;
}
