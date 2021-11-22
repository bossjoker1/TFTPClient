#include<iostream>
#include<windows.h>
#include<atlstr.h >
#include<winsock.h>  
#include<time.h>
#include<string>

#include "client.h"
using namespace std;
#pragma comment(lib, "ws2_32.lib") 

// ȫ�ֱ���
string serverIP;
int cli_socket;
struct sockaddr_in server_addr;
int time_out = TIME_OUT;


time_t now;

// ��־�ļ�ָ��
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
		cout << "����־�ļ�ʧ�ܣ�" << endl;
		return 0;
	}

	while (c) {
		printf("----------------TFTPӦ�ó���----------------\n");
		printf("-------------1.��������IP-----------------\n");
		printf("-------------2.�ϴ��ļ�����-----------------\n");
		printf("-------------3.�����ļ�����-----------------\n");
		printf("-------------0. �˳�------------------------\n");
		cin >> c;
		switch (c)
		{
		case 1:
			cout << "����������IP��ַ" << endl;
			cin >> serverIP;
			writeLog("���÷�����IPΪ��" + serverIP, time(&now));
			flag = true;
			break;
		case 2:
			if (!flag) {
				cout << "�������÷����IP��" << endl;
				continue;
			}
			initSocket();
			uploadFile();
			closesocket(cli_socket);
			break;
		case 3:
			if (!flag) {
				cout << "�������÷����IP��" << endl;
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
	// fprintfд����stream�У���Ҫ�رպ�����д�뵽�ļ�
	// fwrite(msg.data(), 1, msg.size(),Lfp);
	fprintf(Lfp, "time : %s  log: %s \n\n\n", asctime(lt), msg.c_str());
}

void initSocket() {
	// ��ʼ���׽���
	WSADATA wsaData;
	int nRC;
	//��ʼ�� winsock
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
	// �����ͻ���socket
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
	/* ��������� */
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

	// ʱ��Ͱ��Ĵ�С
	int curtime, recvSize;
	int len = sizeof(sockaddr_in);
	u_long temp;

	tstart = clock();

	do {
		// �ڷ����������ش��ڵ������ɹ�
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
					cout << "δ�յ�DATA����" << blockNum << endl;
					writeLog("\nwarning: ��ʱ��δ�յ�DATA��", time(&now));
					return;
				}
				cout << "����DATA�쳣��" << en << endl;
				writeLog("����DATA�쳣", time(&now));
			}
			if (recvSize >= 4 && dp.OPCODE == htons(DATA) && dp.BlockNumber) {
				//data�������ȷ
				if (dp.BlockNumber == htons(blockNum)) {
					cout << "���ܵ�DATA����" << blockNum << endl;

					ack.BlockNumber = dp.BlockNumber;
					// ����ACK
					if (sendto(cli_socket, (char*)&ack, sizeof(ack), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
						cout << "Send ACK Failed:" << endl;
						int en = WSAGetLastError();
						cout << en << endl;
						writeLog("\n error: ����ACKʧ�ܣ��������Ϊ: " + en, time(&now));
						break;
					}
					// д���ļ�
					fwrite(dp.data, 1, recvSize - 4, fp);

					break;
				}
				else {
					cout << "���ܵ�����DATA��" << endl;
					ack.BlockNumber = htons(blockNum-1); // ������һ�ε�ACK
					if (sendto(cli_socket, (char*)&ack, sizeof(ack), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
						cout << "Send ACK Failed:" << endl;
						int en = WSAGetLastError();
						cout << en << endl;
						writeLog("\n error: ����ACKʧ�ܣ��������Ϊ: " + en, time(&now));
						break;
					}
				}
			}
		}
		// �����ش���δ��

		sumSize += (recvSize - 4);
		blockNum++;
	} while (recvSize == DATA_SIZE + 4);

	tend = clock();
	t = (double)(tend - tstart) / CLK_TCK;
	cout << "�����ļ��ɹ�!" << endl;
	printf("�����ļ��Ĵ�СΪ %.2f KB \n", sumSize / 1024);
	printf("�����ٶ�: %.1f KB/s \n", sumSize / (1024 * t));
	char msg[512];
	sprintf(msg, "�����ļ��ɹ���\n�����ļ��Ĵ�СΪ %.2f KB\n �����ٶ�Ϊ��%.1f KB/s", sumSize / 1024, sumSize / (1024 * t));
	string t2(msg);
	writeLog(t2, time(&now));
}

// �����Ƿ�����ȷ�յ�ACK
bool recvACK(int num) {
	bool flag = false;
	ACKPackage ack;
	// ʱ��Ͱ��Ĵ�С
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
					cout << "��ʱ��δ���ܵ�ACK" << endl;
					writeLog("\nwarning: ��ʱ��δ�յ�ACK", time(&now));
					break;
				}
				cout << "����ACK�쳣��" << en << endl;
				char msg[512];
				sprintf(msg, "����ACK %d num �쳣��������� %d\n", num, en);
				string t2(msg);
				writeLog(t2, time(&now));
				//}
			}
		}

		// ���յ�ACKΪ4�ֽڣ��������ȡ���ڵ���4
		if (recvSize >= 4 && ack.OPCODE == htons(ACK) && ack.BlockNumber == htons(num) ){
			cout << "�ɹ��յ�ACK = " << num << endl;
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
	// ƴ���ļ���
	strcat(package + 2, filename.data());

	// ģʽ��Ӧ���ֶ��ļ��ķ�ʽ
	if (mode == 1) {
		strcat(package + 3 + filename.size(), "netascii");
	}
	else {
		strcat(package + 3 + filename.size(), "octet");
	}

	/* �����ļ��� */
	if (sendto(cli_socket, package, 512, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		cout << "Send File Name Failed:" << endl;
		int en = WSAGetLastError();
		cout << en << endl;
		writeLog("\n error: ����WRQ/RRQʧ�ܣ��������Ϊ: " + en , time(&now));
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
		// ��ʼ����
		cout << "�������ݰ���" << blockNum << endl;
		if (sendto(cli_socket, (char*)&dp, dataSize + 4, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
			cout << "Send data Failed:" << endl;
			int en = WSAGetLastError();
			cout << en << endl;
			writeLog("\n error: ����DATA��ʧ�ܣ��������Ϊ: " + en, time(&now));
			return;
		}

		// �������յ�һ��
		tag = recvACK(blockNum);
		// ����ش�����
		for (cnt; cnt <= 3 && !tag; cnt++) {
			// �����ش�
			cout << "�ش���" << cnt << "��" << endl;
			sendto(cli_socket, (char*)&dp, dataSize + 4, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
			char msg[512];
			sprintf(msg, "�� %d ���ش� %d ��data��", cnt, blockNum);
			string temp(msg);
			writeLog(temp, time(&now));

			tag = recvACK(blockNum);
		}

		if (!tag) {
			cout << "�ش�������δ�յ�ACK" << endl;
			writeLog("�ش�������δ�յ�ACK", time(&now));
			return;
		}

		blockNum++;
	} while (dataSize == DATA_SIZE); // ����˵�������һ�����ݰ�
	
	tend = clock();
	t = (double)(tend - tstart) / CLK_TCK;
	cout << "�ļ��ɹ��ϴ�!" << endl;
	printf("�ϴ��ļ��Ĵ�СΪ %.2f KB \n", sumSize/1024);
	printf("�ϴ��ٶ�: %.1f KB/s \n", sumSize / (1024 * t));

	char msg[512];
	sprintf(msg, "�ϴ��ļ��ɹ���\n�ϴ��ļ��Ĵ�СΪ %.2f KB\n �ϴ��ٶ�Ϊ��%.1f KB/s", sumSize / 1024, sumSize / (1024 * t));
	string t2(msg);
	writeLog(t2, time(&now));
}

// �ϴ��ļ�
void uploadFile() {
	string filename; int mode;
	string file; FILE* f;
	string temp;
	cout << "��������Ҫ����ı����ļ�·����" << endl;
	cin >> file;
	cout << "�����봫���������� 1: netascii, 0: octet" << endl;
	cin >> mode;
	cout << "�������ϴ���Զ�˵��ļ����·����" << endl;
	cin >> filename;
	if (mode == 1) {
		// �ı���ʽ���ļ�
		f = fopen(file.data(), "r");
		temp = "netascii";
	}
	else {
		// �����Ƹ�ʽ
		f = fopen(file.data(), "rb");
		temp = "octet";
	}
	if (f == NULL) {
		cout << "file not exists!" << endl;
		writeLog("error:" + file + "file not exists.", time(&now));
		return;
	}
	writeLog("�û��ϴ��ļ���" + filename + ", " + "ģʽ��" + temp ,time(&now));
	// ����WRQ
	sendWRRQ(filename, mode, WRQ);
	// ����ACK_0
	if (recvACK(0)) {            
		cout << "���ܵ�ACK_0" << endl;
		// ��ʼ�������ݰ�
		sendData(f);
	}

	fclose(f);
}

// �����ļ�
void downloadFile(){
	string filename, savefile, temp;
	int mode;
	FILE* fp = NULL;

	cout << "����������Ҫ���ص��ļ���" << endl;
	cin >> filename;
	cout << "�����������ļ��������� 1: netascii, 0: octet" << endl;
	cin >> mode;
	cout << "�����뱣�浽���ص��ļ����·����" << endl;
	cin >> savefile;

	if (mode == 1) {
		fp = fopen(savefile.data(), "w");
		temp = "netascii";
	}
	else {
		fp = fopen(savefile.data(), "wb");
		temp = "octet";
	}

	// ��ʼ����RRQ
	sendWRRQ(filename, mode, RRQ);
	recvDataAndSendAck(fp);
	writeLog("�û��ϴ��ļ���" + filename + ", " + "ģʽ��" + temp, time(&now));
	fclose(fp);
}


