#pragma once

#define DATA_SIZE 512
#define FILENAME_SIZE 512
#define TPORT 69
#define TIME_OUT 3000

// ������ 2�ֽڼ�short
#define RRQ		short(1)
#define WRQ		short(2)
#define DATA	short(3)
#define ACK 	short(4)
#define ERROR	short(5)

// ������ Ҳ��2�ֽ�
#define NOTDEFINE		short(0)
#define FILENOTFOUND	short(1)
#define ACCESSVIOLATION short(2)
#define DISKFULL		short(3)
#define ILLEGALOP		short(4)
#define UNKNOWNID		short(5)
#define FILEEXIST		short(6)
#define NOUSER			short(7)

//// ������ṹ

struct ACKPackage
{
	unsigned short OPCODE, BlockNumber; // 2�ֽڲ�����
};

struct DATAPackage
{
	unsigned short OPCODE, BlockNumber; // 2�ֽڲ�����
	char data[DATA_SIZE];
};

//
//struct ERRORPackage
//{
//	unsigned int OPCODE, ErrorCode; // 2�ֽڲ�����
//	char* errorMessage;
//};