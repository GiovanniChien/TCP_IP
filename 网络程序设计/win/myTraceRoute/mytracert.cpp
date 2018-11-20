#include "winsock2.h"
#include "ws2tcpip.h"

#include "stdio.h"
#include "stdlib.h"



#define IP_RECORD_ROUTE  0x7

// 
// IP 包头结构定义
//

typedef struct _iphdr 
{
    unsigned int   h_len:4;        // 头长
    unsigned int   version:4;      // IP版本号
    unsigned char  tos;            // TOS
    unsigned short total_len;      // 包总长
    unsigned short ident;          // ID
    unsigned short frag_and_flags; // 标记
    unsigned char  ttl;            // TTL
    unsigned char  proto;          // 协议 (TCP, UDP 等)
    unsigned short checksum;       // IP 校验和

    unsigned int   sourceIP;
    unsigned int   destIP;
} IpHeader;


#define ICMP_ECHO        8
#define ICMP_ECHOREPLY   0
#define ICMP_MIN         8 // 请求与应答的最小的ICMP包的长度：8-byte ICMP 包头

//
// ICMP 包头
//
typedef struct _icmphdr 
{
    BYTE   i_type;
    BYTE   i_code;                
    USHORT i_cksum;
    USHORT i_id;
    USHORT i_seq;
    // 自己定义的ICMP数据区
    ULONG  timestamp;
} IcmpHeader;

//
// IP 头部选项- 用于 IP_OPTIONS
//
typedef struct _ipoptionhdr
{
    unsigned char        code;        // 选项类型
    unsigned char        len;         // 选项头长
    unsigned char        ptr;         // 选项偏移
    unsigned long        addr[9];     // IP地址列表
} IpOptionHeader;

#define DEF_PACKET_SIZE  32        
#define MAX_PACKET       1024      // 最大的 ICMP 包长
#define MAX_IP_HDR_SIZE  60        // 最大的 IP 头长

BOOL  bRecordRoute;
int   datasize;
char *lpdest;
SOCKADDR_IN prev;
int ttl=1;
u_long time=0;

//打印程序的使用方法
void usage(char *progname)
{
    printf("用法: ping -r <host> [data size]\n");
    printf("       -r           记录路由\n");
    printf("        host        要ping的远程机器的\n");
    printf("        datasize    数据大小。最大1K\n");
    ExitProcess(-1);
}




int SetTtl(SOCKET s, int ttl)
{
    int     optlevel,
            option,
            rc;

    rc = NO_ERROR;
    optlevel = IPPROTO_IP;
    option   = IP_TTL;
   
    rc = setsockopt(
                s,
                optlevel,
                option,
                (char *)&ttl,
                sizeof(ttl)
                );
   
    if (rc == SOCKET_ERROR)
    {
        fprintf(stderr, "SetTtl: setsockopt failed: %d\n", WSAGetLastError());
    }
    return rc;
}

//比较两个IP是否相同
int IsSockaddrEqual(SOCKADDR_IN sa1, SOCKADDR_IN sa2)
{
    

    return memcmp(&sa1.sin_addr,&sa2.sin_addr,sizeof(sa1.sin_addr)) == 0 ;
   
}

//分析接受到的IP包
int AnalyzePacket(char *buf, int bytes)
{
    int     hdrlen=0,
            routes=0,
            rc;

    rc = NO_ERROR;
    
        IpHeader       *v4hdr=NULL;
        IcmpHeader       *icmphdr=NULL;

        v4hdr = (IpHeader *)buf;
        hdrlen = (v4hdr->h_len) * 4;

        if (v4hdr->proto == IPPROTO_ICMP)
        {
            icmphdr = (IcmpHeader *)&buf[hdrlen];

          
                printf("Received ICMP message type %d instead of TTL expired!\n", icmphdr->i_type);
               
        }
    
   
    return rc;
}


//填充ICMP数据
void FillICMPData(char *icmp_data, int datasize)
{
    IcmpHeader *icmp_hdr = NULL;
    char       *datapart = NULL;

    icmp_hdr = (IcmpHeader*)icmp_data;
    icmp_hdr->i_type = ICMP_ECHO;        // ICMP查询：请求。
    icmp_hdr->i_code = 0;
    icmp_hdr->i_id = (USHORT)GetCurrentProcessId();
    icmp_hdr->i_cksum = 0;
    icmp_hdr->i_seq = 0;
  
    datapart = icmp_data + sizeof(IcmpHeader);
    //
    // 填充数据
    //
    memset(datapart,'E', datasize - sizeof(IcmpHeader));
}

//计算校验和
USHORT checksum(USHORT *buffer, int size) 
{
    unsigned long cksum=0;

    while (size > 1) 
    {
        cksum += *buffer++;
        size -= sizeof(USHORT);
    }
    if (size) 
    {
        cksum += *(UCHAR*)buffer;
    }
    cksum = (cksum >> 16) + (cksum & 0xffff);
    cksum += (cksum >>16);
    return (USHORT)(~cksum);
}

//IP选项的处理:记录路由选项
void DecodeIPOptions(char *buf, int bytes)
{
    IpOptionHeader *ipopt = NULL;
    IN_ADDR         inaddr;
    int             i;
    HOSTENT        *host = NULL;

    ipopt = (IpOptionHeader *)(buf + 20);

    printf("RR:   ");
    for(i = 0; i < (ipopt->ptr / 4) - 1; i++)
    {
        inaddr.S_un.S_addr = ipopt->addr[i];
        if (i != 0)
            printf("      ");
        host = gethostbyaddr((char *)&inaddr.S_un.S_addr,
                    sizeof(inaddr.S_un.S_addr), AF_INET);
        if (host)
            printf("(%-15s) %s\n", inet_ntoa(inaddr), host->h_name);
        else
            printf("(%-15s)\n", inet_ntoa(inaddr));
    }
    return;
}

//ICMP头部选项处理

void DecodeICMPHeader(char *buf, int bytes,  struct sockaddr_in *from)
{
    IpHeader       *iphdr = NULL;
    IcmpHeader     *icmphdr = NULL;
    unsigned short  iphdrlen;
    DWORD           tick;
    static   int    icmpcount = 0;

   
  
   
    iphdr = (IpHeader *)buf;
	// 以32位为一个单位，故要*4
    iphdrlen = iphdr->h_len ;

   icmphdr = (IcmpHeader*)(buf + iphdrlen);

    if (IsSockaddrEqual(prev,*from)) 
	{
		return;
    }
	memcpy(&prev,from,sizeof(prev));
	
    printf("ttl:%d  从 %s: 处接受了 %d 字节 ",ttl-1, inet_ntoa(from->sin_addr),bytes);
    printf(" 时间: %u ms", GetTickCount()-time);
    printf("\n");

    icmpcount++;
	 memcpy(&prev,from,sizeof(prev));
    return;
}

void ValidateArgs(int argc, char **argv)
{
    int                i;

    bRecordRoute = FALSE;
    lpdest = NULL;
    datasize = DEF_PACKET_SIZE;
    
    for(i = 1; i < argc; i++)
    {
        if ((argv[i][0] == '-') || (argv[i][0] == '/'))
        {
            switch (tolower(argv[i][1]))
            {
                case 'r':        // 记录路由
                    bRecordRoute = TRUE;
                    break;
                default:
                    usage(argv[0]);
                    break;
            }
        }
        else if (isdigit(argv[i][0]))
            datasize = atoi(argv[i]);
        else
            lpdest = argv[i];
    }
}


        
//主程序　
int main(int argc, char **argv)
{
    
    WSADATA            wsaData;
    SOCKET             sockRaw = INVALID_SOCKET;
    struct sockaddr_in dest,
                       from;
    int                bread,
                       fromlen = sizeof(from),
                       timeout = 10000,
                       ret;
    char              *icmp_data = NULL,
                      *recvbuf = NULL;
    unsigned int       addr = 0;
    USHORT             seq_no = 0;
    struct hostent    *hp = NULL;
    IpOptionHeader     ipopt;
    

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup() failed: %d\n", GetLastError());
        return -1;
    }
    ValidateArgs(argc, argv);

	sockRaw = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockRaw == INVALID_SOCKET) 
    {
        printf("WSASocket() failed: %d\n", WSAGetLastError());
        return -1;
    }
    
    // 设置超时选项
    //
    bread = setsockopt(sockRaw, SOL_SOCKET, SO_RCVTIMEO, 
                (char*)&timeout, sizeof(timeout));

    if(bread == SOCKET_ERROR) 
    {
        printf("setsockopt(SO_RCVTIMEO) failed: %d\n", 
            WSAGetLastError());
        return -1;
    }
    timeout = 10000;
    bread = setsockopt(sockRaw, SOL_SOCKET, SO_SNDTIMEO, 
                (char*)&timeout, sizeof(timeout));

    if (bread == SOCKET_ERROR) 
    {
        printf("setsockopt(SO_SNDTIMEO) failed: %d\n", 
            WSAGetLastError());
        return -1;
    }

    memset(&dest, 0, sizeof(dest));
    
    dest.sin_family = AF_INET;
    if ((dest.sin_addr.s_addr = inet_addr(lpdest)) == INADDR_NONE)
    {//若不是点分十进制表示法（即：表明是域名表示的。）
        if ((hp = gethostbyname(lpdest)) != NULL)
        {
            memcpy(&(dest.sin_addr), hp->h_addr, hp->h_length);
            dest.sin_family = hp->h_addrtype;
            printf("目标机器的IP[dest.sin_addr] = %s \n", inet_ntoa(dest.sin_addr));
        }
        else
        {
            printf("gethostbyname() failed: %d\n", 
                WSAGetLastError());
            return -1;
        }
    }        

    // 
    // 创建一个ICMP包
    //       
    datasize += sizeof(IcmpHeader);  //ICMP数据＋头长

    icmp_data = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                  MAX_PACKET);//发送用
    recvbuf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                  MAX_PACKET);//接受用
    if (!icmp_data) 
    {
        printf("HeapAlloc() failed: %d\n", GetLastError());
        return -1;
    }
    memset(icmp_data,0,MAX_PACKET);//发送包清０
    FillICMPData(icmp_data,datasize);//填充ICMP包头:是ICMP查询请求类型。
    //
    // 发送和接受ICMP包
    //

    prev.sin_addr.s_addr=htonl(INADDR_ANY);
	
    while(1) 
    {
        static int nCount = 0;
        int        bwrote;
        SetTtl(sockRaw,ttl);      
        if (ttl++ >=64) //最多循环64次
           break;
                
        ((IcmpHeader*)icmp_data)->i_cksum = 0;
        ((IcmpHeader*)icmp_data)->timestamp = GetTickCount();
        ((IcmpHeader*)icmp_data)->i_seq = ttl-1;
        ((IcmpHeader*)icmp_data)->i_cksum = 
        checksum((USHORT*)icmp_data, datasize);
         time=GetTickCount();
        //发送ICMP包。IP包头是系统自动加上去的。
        bwrote = sendto(sockRaw, icmp_data, datasize, 0, 
                     (struct sockaddr*)&dest, sizeof(dest));
        if (bwrote == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAETIMEDOUT) 
            {
                printf("发送数据时超时了\n");
                continue;
            }
            printf("sendto() failed: %d\n", WSAGetLastError());
            return -1;
        }
        if (bwrote < datasize) 
        {
            printf("只写了 %d 字节\n", bwrote);
        }

		//接受IP包。包括IP包头.以最大可能的包。此处定义为1K.
        bread = recvfrom(sockRaw, recvbuf, MAX_PACKET, 0, 
                    (struct sockaddr*)&from, &fromlen);
        if (bread == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAETIMEDOUT) 
            {
                printf("接受数据时超时了\n");
                continue;
            }
            printf("recvfrom() 失败: %d\n", WSAGetLastError());
            return -1;
        }

		//对收到的IP包进行分解打印。
	     	
		if (IsSockaddrEqual(prev,from)) 
		{
			break;
		}
		
        DecodeICMPHeader(recvbuf, bread, &from);
        printf("\n"); 
        Sleep(1000);
    }//while
	
    // 释放资源
    //
    if (sockRaw != INVALID_SOCKET) 
        closesocket(sockRaw);
    HeapFree(GetProcessHeap(), 0, recvbuf);
    HeapFree(GetProcessHeap(), 0, icmp_data);

    WSACleanup();
    return 0;
}
