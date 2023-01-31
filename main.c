#include "reg52.h"
#include "lcd.h"
#include "intrins.h"
typedef unsigned int u16;
typedef unsigned char u8;
#define Read_COM 0x01
#define Prog_COM 0x02
#define Erase_COM 0x03
#define En_Wait_TIME 0x81
#define Start_ADDRH 0x20
#define Start_ADDRL 0x00
#define GPIO_KEY P1
#define Max_num 15
sfr ISP_DATA = 0xe2;
sfr ISP_ADDRH = 0xe3;
sfr ISP_ADDRL = 0xe4;
sfr ISP_CMD = 0xe5;
sfr ISP_TRIG = 0xe6;
sfr ISP_CONTR = 0xe7;
u8 alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
u8 blank[] = "                ";
u8 num = 0;
u8 word_t[Max_num];
u8 KeyValue;
u8 keydown_flag = 0;
u8 key1 = 0;
u8 key2 = 0;
u8 key3 = 0;
u8 key4 = 0;
u8 key6 = 0;
u8 cursor = -1;
u8 num_t;
u8 temp;
void ISP_IAP_disable(void);
u8 Byte_read(u16 byte_addr);
void Sector_erase(u16 sector_addr);
void Byte_program(u16 byte_addr, u8 isp_iap_data);
void delay(u16 i)
{
    while (i--)
        ;
}
void DisBlank()
{
    u8 i;
    for (i = 0; i <= 16; i++)
    {
        LcdWriteCom(0x80 + i);
        LcdWriteData(blank[i]);
    }
}
void identify()
{
    if (KeyValue <= 9 && keydown_flag == 0)
    {
        num_t = KeyValue;
        temp = num_t;
        keydown_flag = 1;
    }
    else if (KeyValue <= 9 && keydown_flag == 1)
    {
        num_t = temp * 10 + KeyValue;
        keydown_flag = 0;
    }
    else if (KeyValue > 9 && KeyValue != 12)
    {
        switch (KeyValue)
        {
        case 13:
            keydown_flag = 0;
            num = num_t;
            key1 = 1;
            break;
        case 16:
            key2 = 1;
            break;
        case 10:
            key3++;
            if (key3 != 0)
                DisBlank();
            break;
        case 19:
            key4--;
            DisBlank();
            break;
        case 22:
            key4++;
            DisBlank();
            break;
        case 11:
            key6 = 1;
            DisBlank();
            break;
        }
    }
}
void KeyDown()
{
    char a = 0;
    GPIO_KEY = 0x0f;
    if (GPIO_KEY != 0x0f)
    {
        delay(1000);
        if (GPIO_KEY != 0x0f)
        {
            GPIO_KEY = 0X0F;
            switch (GPIO_KEY)
            {
            case (0X07):
                KeyValue = 1;
                break;
            case (0X0b):
                KeyValue = 2;
                break;
            case (0X0d):
                KeyValue = 3;
                break;
            case (0X0e):
                KeyValue = 13;
                break;
            }
            GPIO_KEY = 0XF0;
            switch (GPIO_KEY)
            {
            case (0X70):
                KeyValue = KeyValue;
                break;
            case (0Xb0):
                KeyValue = KeyValue + 3;
                break;
            case (0Xd0):
                KeyValue = KeyValue + 6;
                break;
            case (0Xe0):
                KeyValue = KeyValue + 9;
                if (KeyValue == 12)
                    KeyValue = 0;
                break;
            }
            while ((a < 50) && (GPIO_KEY != 0xf0))
            {
                delay(1000);
                a++;
            }
            identify();
        }
    }
}
void Display()
{
    u8 flag = 0;
    if (key3 % 2 == 0)
    {
        if (key1 == 1)
        {
            cursor++;
            word_t[cursor] = alpha[num - 1];
            key1 = 0;
        }
        LcdWriteCom(0x80 + cursor);
        LcdWriteData(alpha[num - 1]);
        LcdWriteCom(0xc0);
        LcdWriteData('S');
        LcdWriteCom(0xc1);
        LcdWriteData('A');
        LcdWriteCom(0xc2);
        LcdWriteData('V');
        LcdWriteCom(0xc3);
        LcdWriteData('E');
    }
}
//该段程序将输入的单词保存进入EEPROM字节中，大体思路为预先读取扇区初始地址0x2000，若为空则将单词首字母保存进入该字节且将剩余字母保存进入后续地址，否则读取下一位地址，若为空则保存，不为空继续向下一位读取数据，将该步骤重复进行直至将单词保存进入为空的字节中。在保存时每一个单词加一个分隔符号以便进行后续的显示及删除设计。
//地址中数据若为字母则读取返回值在30-100区间，分隔符号返回值在<30区间，空地址返回值在>100区间，三种不同的返回值可以便于判定
void Save()
{
    u8 i;
    u8 j;
    u8 temp;
    u8 ttemp;
    if (key3 % 2 == 0)  //当前模式为录入模式则运行以下代码进行单词保存
    {
        if (key2 == 1)  //检测保存按键按下
        {
            DisBlank();  //将LCD1602上显示数据清空
            temp = Byte_read(0x2000);  //读取扇区初始地址数据
            if (temp > 100)  //若返回值大于100即该地址为空则直接保存进入当前地址
                i = -1;
            else  //否则依次读取后续地址中的数据直至找到空地址
            {
                for (i = 0;; i++)
                { 
                    temp = Byte_read(0x2000 + i);
                    ttemp = Byte_read(0x2000 + i + 1);
                    if (temp < 30 && ttemp > 100)
                        break;
                }
            }
            j = i + 1;
            for (i = j; i <= j + cursor; i++)  //将预保存数组中的单词保存进入EEPROM，一个地址一个字母。（在输入单词时即将单词保存进入预保存数组）
            {
                Byte_program(0x2000 + i, word_t[i - j]);
            }
            Byte_program(0x2000 + j + cursor + 1, 10);  //在每一个单词结尾加一个分隔符号保存
            cursor = -1;  //重置光标
            key2 = 0;  //重置保存按键
        }
    }
}
//该段程序为将EEPROM中保存的单词显示在LCD1602上。大体思路为：预先将key4定义为需要显示的单词的序号，key4\key5调节序号值大小，从初始地址起始依次读取数据，读取到分隔符号则计数加一，当计数达到序号则将单词显示在LCD1602上
void Display_eeprom()
{
    u8 i;
    u8 m;
    u8 temp;
    u8 ttemp;
    u8 count = 0;
    u8 read_temp;
    if (key3 % 2 == 1)  //若当前模式为查阅模式则运行后续代码
    {
        m = Byte_read(0x2000);  //读取初始地址
        if (m > 100)  //为空则显示空白
        {
            DisBlank();
        }
        else
        {
            if (key4 == 0)  //若序号值为0则显示第一个单词
            {
                for (i = 0;; i++)  //从初始数据开始读取直至读取到分隔符号则停止循环
                {
                    m = Byte_read(0x2000 + i);
                    if (m < 30)
                        break;
                }
                temp = i - 1;  //记录停止循环时的前一位地址即单词尾字母地址
                for (i = 0; i <= temp; i++)  //显示单词
                {
                    read_temp = Byte_read(0x2000 + i);
                    LcdWriteCom(0x80 + i);
                    LcdWriteData(read_temp);
                }
            }
            else  //若序号值不为0
            {
                for (i = 0;; i++)
                {
                    m = Byte_read(0x2000 + i);  //从初始地址开始读取，读取到分割符号则计数+1，计数达到序号值后停止循环
                    if (m < 30)
                        count++;
                    if (count == key4)
                        break;
                }
                temp = i + 1;  //保存停止循环时后一位地址即需要显示单词的首字母地址
                for (i = temp;; i++)  //从单词首字母地址开始读取数据直至分隔符号停止循环
                {
                    m = Byte_read(0x2000 + i);
                    if (m < 30)
                        break;
                }
                ttemp = i - 1;  //保存停止循环时的前一位地址即单词尾字母地址
                for (i = temp; i <= ttemp; i++)  //显示单词
                {
                    read_temp = Byte_read(0x2000 + i);
                    LcdWriteCom(0x80 + i - temp);
                    LcdWriteData(read_temp);
                }
            }
        }
        //在LCD1602第二行显示当前模式
        LcdWriteCom(0xc0);
        LcdWriteData('R');
        LcdWriteCom(0xc1);
        LcdWriteData('E');
        LcdWriteCom(0xc2);
        LcdWriteData('A');
        LcdWriteCom(0xc3);
        LcdWriteData('D');
    }
}
//该段程序为删除当前显示的单词，大体思路为：EEPROM里共八个扇区，将第一个扇区作为保存数据的扇区，最后一个扇区为删除时放置保存数据的temp扇区（因EEPROM擦除数据以扇区为单位擦除）。从初始地址开始读取数据读取到分隔符号则计数+1，计数达到序号值则停止循环设置断点1，将断点1前的所有单词保存进入temp扇区，再从断点1地址开始读取数据读取到分隔符号则设置断点2，将断点2后所有单词保存进入temp扇区，最后擦除保存扇区，将temp扇区数据重新保存至保存扇区。
void Delete_eeprom()
{
    u8 i;
    u8 m;
    u8 count;
    u8 temp;
    u8 flag;
    u8 fflag;
    if (key3 % 2 == 1)  //若当前模式为查阅模式则运行后续代码
    {
        if (key6 == 1)  //检测删除按键按下
        {
            Sector_erase(0x2e00);  //擦除temp扇区
            if (key4 == 0) //若序号值为0则将第一个单词后所有单词保存进入temp扇区
            {
                for (i = 0;; i++)
                {
                    m = Byte_read(0x2000 + i);
                    if (m < 30)
                        break;
                }
                flag = i + 1;
                for (i = flag;; i++)
                {
                    m = Byte_read(0x2000 + i);
                    if (m > 100)
                        break;
                    Byte_program(0x2e00 + i - flag, m);
                }
                Sector_erase(0x2000);
                for (i = 0;; i++)
                {
                    m = Byte_read(0x2e00 + i);
                    if (m > 100)
                        break;
                    Byte_program(0x2000 + i, m);
                }
            }
            else  //若序号值不为0则将当前显示单词前面所有单词及后面所有单词保存进入temp扇区
            {
                for (i = 0;; i++)  //从首地址开始读取数据读取到分隔符号则计数+1，计数达到序号值-1则停止循环
                {
                    m = Byte_read(0x2000 + i);
                    if (m < 30)
                        count++;
                    if (count == key4 - 1)
                        break;
                }
                flag = i;  //保存停止循环时地址即当前显示单词前一个单词结尾分隔符号地址
                for (i = 0; i <= flag; i++)  //将当前显示单词前所有单词保存进入temp扇区
                {
                    m = Byte_read(0x2000 + i);
                    Byte_program(0x2e00 + i, m);
                }
                for (i = flag + 1;; i++)  //继续读取数据直至读取到分隔符号则停止循环，此时的地址为当前显示单词的结束符号地址
                {
                    m = Byte_read(0x2000 + i);
                    if (m < 30)
                        break;
                }
                fflag = i + 1;  //保存当前显示单词下一个单词的首字母地址
                temp = fflag - flag - 1;  //保存当前显示单词所占地址数即字母数+分割符号
                if (Byte_read(0x2000 + flag) < 100)  //若当前显示单词不为最后一个单词，则将后续单词保存进入temp扇区
                {
                    for (i = fflag;; i++)
                    {
                        m = Byte_read(0x2000 + i);
                        Byte_program(0x2e00 + i - temp, m);
                        if (m > 100)
                            break;
                    }
                }
                Sector_erase(0x2000);  //擦除保存扇区
                for (i = 0;; i++)  //将temp扇区数据重新保存进入保存扇区
                {
                    m = Byte_read(0x2e00 + i);
                    if (m > 100)
                        break;
                    Byte_program(0x2000 + i, m);
                }
            }
            key6 = 0;  //重置删除按键值
        }
    }
}
void main()
{
    LcdInit();
    while (1)
    {
        KeyDown();
        Display();
        Save();
        Display_eeprom();
        Delete_eeprom();
    }
}
void ISP_IAP_disable(void)
{
    ISP_CONTR = 0x00;
    ISP_CMD = 0x00;
    ISP_TRIG = 0x00;
}
u8 Byte_read(u16 byte_addr)
{
    EA = 0;// 关中断
    ISP_CONTR = En_Wait_TIME;  //开启ISP&IAP，并送等待时间 
    ISP_CMD = Read_COM;  //送字节读命令字
    ISP_ADDRH = (u8)(byte_addr >> 8);  //送地址高字节
    ISP_ADDRL = (u8)(byte_addr & 0x00ff);  //送地址低字节
    ISP_TRIG = 0x46;  //送触发命令字0x46
    ISP_TRIG = 0xB9;  //送触发命令字0xB9
    _nop_();
    ISP_IAP_disable();  //关闭ISP&IAP功能
    EA = 1;  //开中断
    return (ISP_DATA);  //返回数据
}
void Byte_program(u16 byte_addr, u8 isp_iap_data)
{
    EA = 0;  //关中断    
    ISP_CONTR = En_Wait_TIME;  //开启ISP&IAP，并送等待时间    
    ISP_CMD = Prog_COM;   //送字节编程命令字      
    ISP_ADDRH = (uchar)(byte_addr >> 8);  //送地址高字节    
    ISP_ADDRL = (uchar)(byte_addr & 0x00ff);  //送地址低字节        
    ISP_DATA = isp_iap_data;   //送数据进ISP_DATA    
    ISP_TRIG = 0x46;    //送触发命令字0x46   
    ISP_TRIG = 0xB9;	//送触发命令字0xB9   
    _nop_();   
    ISP_IAP_disable();   //关闭ISP&IAP功能    
    EA = 1;  //开中断     
}
//该段程序为擦除EEPROM整个扇区的数据
void Sector_erase(u16 sector_addr)
{
    EA = 0;   //关中断
    ISP_CONTR = En_Wait_TIME;  //开启ISP&IAP;并送等待时间
    ISP_CMD = Erase_COM;  //送扇区擦除命令字
    ISP_ADDRH = (u8)(sector_addr >> 8);  //送地址高字节
    ISP_ADDRL = (u8)(sector_addr & 0X00FF);  //送地址低字节
    ISP_TRIG = 0X46;  //送触发命令字0x46
    ISP_TRIG = 0XB9;  //送触发命令字0xB9
    _nop_();
    ISP_IAP_disable();  //关闭ISP&IAP功能
    EA = 1;  //开中断
}

