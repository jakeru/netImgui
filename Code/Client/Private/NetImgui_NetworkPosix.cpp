#include "NetImgui_Shared.h"

#if defined(_MSC_VER) 
#pragma warning (disable: 4221)		
#endif

#if NETIMGUI_ENABLED && NETIMGUI_POSIX_SOCKETS_ENABLED
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <stdio.h>
#include <errno.h>
#include <poll.h>

namespace NetImgui { namespace Internal { namespace Network 
{

struct SocketInfo
{
	SocketInfo(int socket) : mSocket(socket){}
	int mSocket;
};

bool Startup()
{
	return true;
}

void Shutdown()
{
}

static void SetNonBlocking(int Socket, bool bConfigureAsNonBlocking)
{
	int Flags	= fcntl(Socket, F_GETFL, 0);
	if (bConfigureAsNonBlocking) {
		Flags |= O_NONBLOCK;
	}
	else {
		Flags &= ~O_NONBLOCK;
	}
	fcntl(Socket, F_SETFL, Flags);
}

static bool WaitForConnectResult(int Socket, int Timeout_ms)
{
	struct pollfd pfds = { .fd = Socket, .events = POLLOUT };

	int poll_res;
	while ((poll_res = poll(&pfds, 1, Timeout_ms) == EINTR)) {
		// Interrupted. Try again.
	}

	if (poll_res <= 0) {
		// If poll_res == 0: Timeout
		// If poll_res < 0: Other error, lookup `errno` in errno.h 
		return false;
	}

	int error = -1;
	socklen_t len = sizeof(error);
	const int res = getsockopt(Socket, SOL_SOCKET, SO_ERROR, &error, &len);
	if (res != 0 || error != 0) {
		// Error.
		// If res != 0: lookup `errno` in errno.h.
		// Elif error != 0: lookup `error` in errno.h.
		return false;
	}

	return true;
}

static bool ConnectWithTimeout(int Socket, const addrinfo* pAddr, int Timeout_ms)
{
	SetNonBlocking(Socket, true);

	int res = connect(Socket, pAddr->ai_addr, pAddr->ai_addrlen);
	if (res == 0) {
		return true;
	}
	else if (errno != EINPROGRESS) {
		return false;
	}

	return WaitForConnectResult(Socket, Timeout_ms);
}

SocketInfo* Connect(const char* ServerHost, uint32_t ServerPort)
{
	const int Timeout_ms = 5000;
	const addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	char zPortName[32];
	addrinfo*	pResults	= nullptr;
	SocketInfo* pSocketInfo	= nullptr;
	sprintf(zPortName, "%i", ServerPort);
	const int res = getaddrinfo(ServerHost, zPortName, &hints, &pResults);
	if (res != 0) {
		return nullptr;
	}
	for (addrinfo* pResultCur = pResults; pResultCur != nullptr; pResultCur = pResultCur->ai_next) {
		int ConnectSocket = socket(pResultCur->ai_family, pResultCur->ai_socktype, 0);
		if(ConnectSocket == -1) {
			break;
		}
		if( ConnectWithTimeout(ConnectSocket, pResultCur, Timeout_ms) )
		{
			SetNonBlocking(ConnectSocket, false);
			pSocketInfo = netImguiNew<SocketInfo>(ConnectSocket);
			break;
		}
		close(ConnectSocket);
	}

	freeaddrinfo(pResults);

	return pSocketInfo;
}

SocketInfo* ListenStart(uint32_t ListenPort)
{
	addrinfo hints;

	memset(&hints, 0, sizeof hints);
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_flags		= AI_PASSIVE;

	addrinfo* addrInfo;
	getaddrinfo(nullptr, std::to_string(ListenPort).c_str(), &hints, &addrInfo);

	int ListenSocket = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);
	if( ListenSocket != -1 )
	{
	#if NETIMGUI_FORCE_TCP_LISTEN_BINDING
		int flag = 1;
		setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
		setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag));
	#endif
		if(	bind(ListenSocket, addrInfo->ai_addr, addrInfo->ai_addrlen) != -1 &&
			listen(ListenSocket, 0) != -1)
		{
			SetNonBlocking(ListenSocket, false);
			return netImguiNew<SocketInfo>(ListenSocket);
		}
		close(ListenSocket);
	}
	return nullptr;
}

SocketInfo* ListenConnect(SocketInfo* ListenSocket)
{
	if( ListenSocket )
	{
		sockaddr_storage ClientAddress;
		socklen_t Size(sizeof(ClientAddress));
		int ServerSocket = accept(ListenSocket->mSocket, (sockaddr*)&ClientAddress, &Size) ;
		if (ServerSocket != -1)
		{
			SetNonBlocking(ServerSocket, false);
			return netImguiNew<SocketInfo>(ServerSocket);
		}
	}
	return nullptr;
}

void Disconnect(SocketInfo* pClientSocket)
{
	if( pClientSocket )
	{
		shutdown(pClientSocket->mSocket, SHUT_RDWR);
		close(pClientSocket->mSocket);
		netImguiDelete(pClientSocket);
	}
}

bool DataReceive(SocketInfo* pClientSocket, void* pDataIn, size_t Size)
{
	int resultRcv = recv(pClientSocket->mSocket, static_cast<char*>(pDataIn), static_cast<int>(Size), MSG_WAITALL);
	return static_cast<int>(Size) == resultRcv;
}

bool DataSend(SocketInfo* pClientSocket, void* pDataOut, size_t Size)
{
	int resultSend = send(pClientSocket->mSocket, static_cast<char*>(pDataOut), static_cast<int>(Size), 0);
	return static_cast<int>(Size) == resultSend;
}

}}} // namespace NetImgui::Internal::Network
#else

// Prevents Linker warning LNK4221 in Visual Studio (This object file does not define any previously undefined public symbols, so it will not be used by any link operation that consumes this library)
extern int sSuppresstLNK4221_NetImgui_NetworkPosix; 
int sSuppresstLNK4221_NetImgui_NetworkPosix(0);

#endif // #if NETIMGUI_ENABLED && NETIMGUI_POSIX_SOCKETS_ENABLED

