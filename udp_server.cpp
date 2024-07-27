#include "comm.h"
#include <unistd.h>
#include <thread>
#include "console.h"

#if defined(__MINGW32__)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef int socklen_t;
#else
#include <sys/socket.h>    //for socket ofcourse
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif


#if defined(__MINGW32__)
typedef SOCKET my_fd_t;
inline int sock_close(my_fd_t fd)
{
	return closesocket(fd);
}
#else
typedef int my_fd_t;
inline int sock_close(my_fd_t fd)
{
	return close(fd);
}
#endif


static int udp_fd;
static struct sockaddr_in myaddr;
static struct sockaddr_in remaddr;

void read_loop(std::string msg)
{
	char buf[1000];
	socklen_t addrlen = sizeof(remaddr);  
	while(1){
		ssize_t recvlen = recvfrom(udp_fd, buf, sizeof(buf)-1, 0, (struct sockaddr *)&remaddr, &addrlen);
		if(recvlen>0) {
			buf[recvlen]=0;
		} 
        push_message(buf);

	}
    //std::cout << "task1 says: " << msg;
}

int init_ws()
{
#if defined(__MINGW32__)
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* Winsock DLL.                                  */
		printf("WSAStartup failed with error: %d\n", err);
		exit(-1);
	}

	/* Confirm that the WinSock DLL supports 2.2.*/
	/* Note that if the DLL supports versions greater    */
	/* than 2.2 in addition to 2.2, it will still return */
	/* 2.2 in wVersion since that is the version we      */
	/* requested.                                        */

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		exit(-1);
	}
	else
	{
		printf("The Winsock 2.2 dll was found okay");
	}
	
	int tmp[]={0,100,200,300,500,800,1000,2000,3000,4000,-1};
	int succ=0;
	for(int i=1;tmp[i]!=-1;i++)
	{
		if(_setmaxstdio(100)==-1) break;
		else succ=i;
	}	
	printf(", _setmaxstdio() was set to %d\n",tmp[succ]);
#endif
return 0;
}


void init_udp_server(int listen_port){
    init_ws();

	if ((udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("cannot create socket");
		return ;
	}
	printf("create socket done\n");
	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	myaddr.sin_port = htons(listen_port);

	if (bind(udp_fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		printf("bind failed");
		sock_close(udp_fd);
		return ;
	}
	
	printf("bind socket done\n");

	printf("udp listening at %d!!!\n",listen_port);
	std::thread task1(read_loop, "hi");
	task1.detach();
	return ;
}
