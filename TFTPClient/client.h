#pragma once

#define DATA_SIZE 512
#define FILENAME_SIZE 512
#define TPORT 69
#define TIME_OUT 3000

// 操作码 2字节即short
#define RRQ		short(1)
#define WRQ		short(2)
#define DATA	short(3)
#define ACK 	short(4)
#define ERROR	short(5)

// 错误码 也是2字节
#define NOTDEFINE		short(0)
#define FILENOTFOUND	short(1)
#define ACCESSVIOLATION short(2)
#define DISKFULL		short(3)
#define ILLEGALOP		short(4)
#define UNKNOWNID		short(5)
#define FILEEXIST		short(6)
#define NOUSER			short(7)

//// 定义包结构

struct ACKPackage
{
	unsigned short OPCODE, BlockNumber; // 2字节操作码
};

struct DATAPackage
{
	unsigned short OPCODE, BlockNumber; // 2字节操作码
	char data[DATA_SIZE];
};

//
//struct ERRORPackage
//{
//	unsigned int OPCODE, ErrorCode; // 2字节操作码
//	char* errorMessage;
//};