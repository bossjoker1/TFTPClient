#include<iostream>
#include<windows.h>
#include<atlstr.h >
#include<winsock.h>  
#include<time.h>
#include<string>

#include "client.h"
using namespace std;
#pragma comment(lib, "ws2_32.lib") 

// 全局变量
string serverIP;
int cli_socket;
struct sockaddr_in server_addr;
int time_out = TIME_OUT;


time_t now;

// 日志文件指针
FILE* Lfp =	NULL;

clock_t tstart, tend;

void initSocket();
void sendWRRQ(string filename, int mode, short wrrq);
void uploadFile();
void downloadFile();
bool recvACK(int num);
void sendData(FILE* fp);
void recvDataAndSendAck(FILE* fp);
void writeLog(string msg, time_t t);

int main() {
	int c = 100;
	bool flag = false;
	Lfp = fopen("tftp.log", "a");
	if (Lfp == NULL) {
		cout << "打开日志文件失败！" << endl;
		return 0;
	}

	while (c) {
		printf("----------------TFTP应用程序----------------\n");
		printf("-------------1.输入服务端IP-----------------\n");
		printf("-------------2.上传文件操作-----------------\n");
		printf("-------------3.下载文件操作-----------------\n");
		printf("-------------0. 退出------------------------\n");
		cin >> c;
		switch (c)
		{
		case 1:
			cout << "请输入服务端IP地址" << endl;
			cin >> serverIP;
			writeLog("配置服务器IP为：" + serverIP, time(&now));
			flag = true;
			break;
		case 2:
			if (!flag) {
				cout << "请先配置服务端IP！" << endl;
				continue;
			}
			initSocket();
			uploadFile();
			closesocket(cli_socket);
			break;
		case 3:
			if (!flag) {
				cout << "请先配置服务端IP！" << endl;
				continue;
			}
			initSocket();
			downloadFile();
			closesocket(cli_socket);
			break;
		default:
				break;
		}

	}
	fclose(Lfp);
	WSACleanup();
	return 0;
}

void writeLog(string msg, time_t t) {
	tm* lt = localtime(&t);
	// fprintf写入流stream中，需要关闭后才完成写入到文件
	// fwrite(msg.data(), 1, msg.size(),Lfp);
	fprintf(Lfp, "time : %s  log: %s \n\n\n", asctime(lt), msg.c_str());
}

void initSocket() {
	// 初始化套接字
	WSADATA wsaData;
	int nRC;
	//初始化 winsock
	nRC = WSAStartup(0x0101, &wsaData);
	if (nRC)
	{
		printf("Client initialize winsock error!\n");
		return;
	}
	if (wsaData.wVersion != 0x0101)
	{
		printf("Client's winsock version error!\n");
		WSACleanup();
		return;
	}
	printf("Client's winsock initialized !\n");
	// 创建客户端socket
	cli_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (cli_socket < 0)
	{
		cout << "Create Socket Failed:" << endl;
		exit(1);
	}
	if (cli_socket == INVALID_SOCKET) {
		cout << "Socket() failed!" << endl;
		WSACleanup();
		return;
	}
	/* 服务端配置 */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(serverIP.data());
	server_addr.sin_port = htons(TPORT);
}

void recvDataAndSendAck(FILE* fp){
	double sumSize = 0.0;
	double t;
	ACKPackage ack;
	DATAPackage dp;
	ack.OPCODE = htons(ACK);

	unsigned short blockNum = 1;

	// 时间和包的大小
	int curtime, recvSize;
	int len = sizeof(sockaddr_in);
	u_long temp;

	tstart = clock();

	do {
		// 在服务器三次重传内到达就算成功
		for (;;) {
			time_out = 9000;
			if (setsockopt(cli_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(int)) < 0) {
				cout << "socket option not support!" << endl;
				break;
			}
			recvSize = recvfrom(cli_socket, (char*)&dp, sizeof(DATAPackage), 0, (struct sockaddr*)&server_addr, &len);
			if (recvSize == SOCKET_ERROR) {
				int en = WSAGetLastError();
				if (en == 10060) {
					cout << "未收到DATA包：" << blockNum << endl;
					writeLog("\nwarning: 超时且未收到DATA包", time(&now));
					return;
				}
				cout << "接收DATA异常：" << en << endl;
				writeLog("接收DATA异常", time(&now));
			}
			if (recvSize >= 4 && dp.OPCODE == htons(DATA) && dp.BlockNumber) {
				//data包序号正确
				if (dp.BlockNumber == htons(blockNum)) {
					cout << "接受到DATA包：" << blockNum << endl;

					ack.BlockNumber = dp.BlockNumber;
					// 发送ACK
					if (sendto(cli_socket, (char*)&ack, sizeof(ack), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
						cout << "Send ACK Failed:" << endl;
						int en = WSAGetLastError();
						cout << en << endl;
						writeLog("\n error: 发送ACK失败，错误代码为: " + en, time(&now));
						break;
					}
					// 写入文件
					fwrite(dp.data, 1, recvSize - 4, fp);

					break;
				}
				else {
					cout << "接受到乱序DATA包" << endl;
					ack.BlockNumber = htons(blockNum-1); // 发送上一次的ACK
					if (sendto(cli_socket, (char*)&ack, sizeof(ack), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
						cout << "Send ACK Failed:" << endl;
						int en = WSAGetLastError();
						cout << en << endl;
						writeLog("\n error: 发送ACK失败，错误代码为: " + en, time(&now));
						break;
					}
				}
			}
		}
		// 三次重传内未到

		sumSize += (recvSize - 4);
		blockNum++;
	} while (recvSize == DATA_SIZE + 4);

	tend = clock();
	t = (double)(tend - tstart) / CLK_TCK;
	cout << "下载文件成功!" << endl;
	printf("下载文件的大小为 %.2f KB \n", sumSize / 1024);
	printf("下载速度: %.1f KB/s \n", sumSize / (1024 * t));
	char msg[512];
	sprintf(msg, "下载文件成功！\n下载文件的大小为 %.2f KB\n 下载速度为：%.1f KB/s", sumSize / 1024, sumSize / (1024 * t));
	string t2(msg);
	writeLog(t2, time(&now));
}

// 返回是否能正确收到ACK
bool recvACK(int num) {
	bool flag = false;
	ACKPackage ack;
	// 时间和包的大小
	int curtime, recvSize;
	int len = sizeof(sockaddr_in);
	u_long temp;
	for (;;) {
		time_out = 3000;
		if (setsockopt(cli_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(int)) < 0) {
			cout << "socket option not support!" << endl;
			break;
		}
		// ioctlsocket(cli_socket, FIONBIO, &temp);
		recvSize = recvfrom(cli_socket, (char *)&ack, sizeof(ACKPackage), 0, (struct sockaddr*)&server_addr, &len);
		if (recvSize < 0) {
			if (recvSize == SOCKET_ERROR) {
				int en = WSAGetLastError();
				//if (en != 10035) {
					// if (en == 10035) continue;
				if (en == 10060) {
					cout << "超时，未接受到ACK" << endl;
					writeLog("\nwarning: 超时且未收到ACK", time(&now));
					break;
				}
				cout << "接收ACK异常：" << en << endl;
				char msg[512];
				sprintf(msg, "接收ACK %d num 异常，错误代码 %d\n", num, en);
				string t2(msg);
				writeLog(t2, time(&now));
				//}
			}
		}

		// 接收的ACK为4字节，保险起见取大于等于4
		if (recvSize >= 4 && ack.OPCODE == htons(ACK) && ack.BlockNumber == htons(num) ){
			cout << "成功收到ACK = " << num << endl;
			flag = true;
			break;
		}

	}

	return flag;
}
 
void sendWRRQ(string filename, int mode, short wrrq) {
	char package[DATA_SIZE];
	memset(package, 0, sizeof(package));
	package[0] = 0, package[1] = wrrq;
	// 拼接文件名
	strcat(package + 2, filename.data());

	// 模式对应两种读文件的方式
	if (mode == 1) {
		strcat(package + 3 + filename.size(), "netascii");
	}
	else {
		strcat(package + 3 + filename.size(), "octet");
	}

	/* 发送文件名 */
	if (sendto(cli_socket, package, 512, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		cout << "Send File Name Failed:" << endl;
		int en = WSAGetLastError();
		cout << en << endl;
		writeLog("\n error: 发送WRQ/RRQ失败，错误代码为: " + en , time(&now));
		return;
	}
	return;
}

void sendData(FILE* fp) {
	int dataSize = 0;
	double sumSize = 0.0;
	double t;
	unsigned short blockNum = 1;
	DATAPackage dp;
	dp.OPCODE = htons(DATA);
	tstart = clock();
	bool tag;
	do {
		int cnt = 1;
		memset(dp.data, 0, sizeof(dp.data));
		dp.BlockNumber = htons(blockNum);
		dataSize = fread(dp.data, 1, DATA_SIZE, fp);
		sumSize += dataSize;
		// 开始发送
		cout << "发送数据包：" << blockNum << endl;
		if (sendto(cli_socket, (char*)&dp, dataSize + 4, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
			cout << "Send data Failed:" << endl;
			int en = WSAGetLastError();
			cout << en << endl;
			writeLog("\n error: 发送DATA包失败，错误代码为: " + en, time(&now));
			return;
		}

		// 正常接收第一次
		tag = recvACK(blockNum);
		// 最多重传三次
		for (cnt; cnt <= 3 && !tag; cnt++) {
			// 三秒重传
			cout << "重传第" << cnt << "次" << endl;
			sendto(cli_socket, (char*)&dp, dataSize + 4, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
			char msg[512];
			sprintf(msg, "第 %d 次重传 %d 号data包", cnt, blockNum);
			string temp(msg);
			writeLog(temp, time(&now));

			tag = recvACK(blockNum);
		}

		if (!tag) {
			cout << "重传三次仍未收到ACK" << endl;
			writeLog("重传三次仍未收到ACK", time(&now));
			return;
		}

		blockNum++;
	} while (dataSize == DATA_SIZE); // 不等说明是最后一个数据包
	
	tend = clock();
	t = (double)(tend - tstart) / CLK_TCK;
	cout << "文件成功上传!" << endl;
	printf("上传文件的大小为 %.2f KB \n", sumSize/1024);
	printf("上传速度: %.1f KB/s \n", sumSize / (1024 * t));

	char msg[512];
	sprintf(msg, "上传文件成功！\n上传文件的大小为 %.2f KB\n 上传速度为：%.1f KB/s", sumSize / 1024, sumSize / (1024 * t));
	string t2(msg);
	writeLog(t2, time(&now));
}

// 上传文件
void uploadFile() {
	string filename; int mode;
	string file; FILE* f;
	string temp;
	cout << "请输入所要传入的本机文件路径：" << endl;
	cin >> file;
	cout << "请输入传入数据类型 1: netascii, 0: octet" << endl;
	cin >> mode;
	cout << "请输入上传到远端的文件相对路径：" << endl;
	cin >> filename;
	if (mode == 1) {
		// 文本形式读文件
		f = fopen(file.data(), "r");
		temp = "netascii";
	}
	else {
		// 二进制格式
		f = fopen(file.data(), "rb");
		temp = "octet";
	}
	if (f == NULL) {
		cout << "file not exists!" << endl;
		writeLog("error:" + file + "file not exists.", time(&now));
		return;
	}
	writeLog("用户上传文件：" + filename + ", " + "模式：" + temp ,time(&now));
	// 发送WRQ
	sendWRRQ(filename, mode, WRQ);
	// 接收ACK_0
	if (recvACK(0)) {            
		cout << "接受到ACK_0" << endl;
		// 开始发送数据包
		sendData(f);
	}

	fclose(f);
}

// 下载文件
void downloadFile(){
	string filename, savefile, temp;
	int mode;
	FILE* fp = NULL;

	cout << "请输入你想要下载的文件：" << endl;
	cin >> filename;
	cout << "请输入下载文件数据类型 1: netascii, 0: octet" << endl;
	cin >> mode;
	cout << "请输入保存到本地的文件相对路径：" << endl;
	cin >> savefile;

	if (mode == 1) {
		fp = fopen(savefile.data(), "w");
		temp = "netascii";
	}
	else {
		fp = fopen(savefile.data(), "wb");
		temp = "octet";
	}

	// 开始发送RRQ
	sendWRRQ(filename, mode, RRQ);
	recvDataAndSendAck(fp);
	writeLog("用户上传文件：" + filename + ", " + "模式：" + temp, time(&now));
	fclose(fp);
}


