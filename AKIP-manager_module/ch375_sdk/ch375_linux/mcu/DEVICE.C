/* 
* CH374芯片应用层
* Copyright (C) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
* Web:      http://wch.cn
* Author:   WCH <tech@wch.cn>
* USB设备,模拟CH372或CH375的TEST程序与计算机通讯
 */

#include	"..\HAL.H"			// 以MCS51为例，其它单片机需修改HAL*硬件抽象层的几个文件
#include	"..\HAL_BASE.C"	// 基本子程序及中断查询子程序

/* 硬件接口层,以下连接方式任选一种 */
#include "..\PARA_HW.C"	/* 硬件标准8位并口 */
//#include "..\PARA_SW.C"	/* 软件I/O模拟8位并口 */
//#include "..\SPI_HW.C"	/* 硬件标准4线SPI串口 */
//#include "..\SPI_SW.C"	/* 软件I/O模拟4线SPI串口 */
//#include "..\SPI3_SW.C"	/* 软件I/O模拟3线SPI串口,SDO和SDI合用一个引脚 */

// 设备描述符
const	UINT8C	MyDevDescr[] = { 0x12, 0x01, 0x10, 0x01,
								0xFF, 0x80, 0x37, 0x08,
								0x48, 0x43, 0x37, 0x55,  // 厂商ID和产品ID
								0x00, 0x01, 0x01, 0x02,
								0x00, 0x01 };
// 配置描述符
const	UINT8C	MyCfgDescr[] = { 0x09, 0x02, 0x27, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
								 0x09, 0x04, 0x00, 0x00, 0x03, 0xFF, 0x80, 0x37, 0x00,
								 0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00,
								 0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
								 0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x00 };
// 语言描述符
const	UINT8C	MyLangDescr[] = { 0x04, 0x03, 0x09, 0x04 };
// 厂家信息
const	UINT8C	MyManuInfo[] = { 0x0E, 0x03, 'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0 };
// 产品信息
const	UINT8C	MyProdInfo[] = { 0x0C, 0x03, 'C', 0, 'H', 0, '3', 0, '7', 0, '4', 0 };

UINT8	UsbConfig = 0;	// USB配置标志

void	USB_DeviceInterrupt( void );  // USB设备中断服务程序

void	Init374Device( void );  // 初始化USB设备


void	USB_DeviceInterrupt( void )  // USB设备中断服务程序
{
	UINT8	s, l;
	static	UINT8	SetupReq, SetupLen;
	static	PUINT8	pDescr;
	s = Read374Byte( REG_INTER_FLAG );  // 获取中断状态
	if ( s & BIT_IF_BUS_RESET ) {  // USB总线复位
		Write374Byte( REG_USB_ADDR, 0x00 );  // 清USB设备地址
		Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( 0 ) );
		Write374Byte( REG_USB_ENDP1, M_SET_EP1_TRAN_NAK( 0 ) );
		Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( 0 ) );
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_BUS_RESET );  // 清中断标志
	}
	else if ( s & BIT_IF_TRANSFER ) {  // USB传输完成
		s = Read374Byte( REG_USB_STATUS );
		switch( s & BIT_STAT_PID_ENDP ) {  // USB设备中断状态
			case USB_INT_EP2_OUT: {  // 批量端点下传成功 
				UINT8	buf[64];
				if ( s & BIT_STAT_TOG_MATCH ) {  // 仅同步包
					l = Read374Byte( REG_USB_LENGTH );
					Read374Block( RAM_ENDP2_RECV, l, buf );
					for ( s = 0; s < l; s ++ ) buf[s] ^= 0xFF;  // 数据取反由计算机验证
					Write374Block( RAM_ENDP2_TRAN, l, buf );  // 演示回传
					Write374Byte( REG_USB_LENGTH, l );
					Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_ACK( Read374Byte( REG_USB_ENDP2 ) ) ^ BIT_EP2_RECV_TOG );
//					Write374Index( REG_USB_ENDP2 );  // 对于并口连接可以用本行及下面一行代替上一行的程序,减少写一次index的时间,提高效率
//					Write374Data( M_SET_EP2_TRAN_ACK( Read374Data0( ) ) ^ BIT_EP2_RECV_TOG );
				}
				break;
			}
			case USB_INT_EP2_IN: {  // 批量端点上传成功,未处理
				Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( Read374Byte( REG_USB_ENDP2 ) ) ^ BIT_EP2_TRAN_TOG );
//				Write374Index( REG_USB_ENDP2 );  // 对于并口连接可以用本行及下面一行代替上一行的程序,减少写一次index的时间,提高效率
//				Write374Data( M_SET_EP2_TRAN_NAK( Read374Data0( ) ) ^ BIT_EP2_TRAN_TOG );
				break;
			}
			case USB_INT_EP1_IN: {  // 中断端点上传成功,未处理
				Write374Byte( REG_USB_ENDP1, M_SET_EP1_TRAN_NAK( Read374Byte( REG_USB_ENDP1 ) ) ^ BIT_EP1_TRAN_TOG );
				break;
			}
			case USB_INT_EP0_SETUP: {  // 控制传输
				USB_SETUP_REQ	SetupReqBuf;
				l = Read374Byte( REG_USB_LENGTH );
				if ( l == sizeof( USB_SETUP_REQ ) ) {
					Read374Block( RAM_ENDP0_RECV, l, (PUINT8)&SetupReqBuf );
					SetupLen = SetupReqBuf.wLengthL;
					if ( SetupReqBuf.wLengthH || SetupLen > 0x7F ) SetupLen = 0x7F;  // 限制总长度
					l = 0;  // 默认为成功并且上传0长度
					if ( ( SetupReqBuf.bType & DEF_USB_REQ_TYPE ) != DEF_USB_REQ_STAND ) {  /* 只支持标准请求 */
						l = 0xFF;  // 操作失败
					}
					else {  // 标准请求
						SetupReq = SetupReqBuf.bReq;  // 请求码
						switch( SetupReq ) {
							case DEF_USB_GET_DESCR:
								switch( SetupReqBuf.wValueH ) {
									case 1:
										pDescr = (PUINT8)( &MyDevDescr[0] );
										l = sizeof( MyDevDescr );
										break;
									case 2:
										pDescr = (PUINT8)( &MyCfgDescr[0] );
										l = sizeof( MyCfgDescr );
										break;
									case 3:
										switch( SetupReqBuf.wValueL ) {
											case 1:
												pDescr = (PUINT8)( &MyManuInfo[0] );
												l = sizeof( MyManuInfo );
												break;
											case 2:
												pDescr = (PUINT8)( &MyProdInfo[0] );
												l = sizeof( MyProdInfo );
												break;
											case 0:
												pDescr = (PUINT8)( &MyLangDescr[0] );
												l = sizeof( MyLangDescr );
												break;
											default:
												l = 0xFF;  // 操作失败
												break;
										}
										break;
									default:
										l = 0xFF;  // 操作失败
										break;
								}
								if ( SetupLen > l ) SetupLen = l;  // 限制总长度
								l = SetupLen >= RAM_ENDP0_SIZE ? RAM_ENDP0_SIZE : SetupLen;  // 本次传输长度
								Write374Block( RAM_ENDP0_TRAN, l, pDescr );  /* 加载上传数据 */
								SetupLen -= l;
								pDescr += l;
								break;
							case DEF_USB_SET_ADDRESS:
								SetupLen = SetupReqBuf.wValueL;  // 暂存USB设备地址
								break;
							case DEF_USB_GET_CONFIG:
								Write374Byte( RAM_ENDP0_TRAN, UsbConfig );
								if ( SetupLen >= 1 ) l = 1;
								break;
							case DEF_USB_SET_CONFIG:
								UsbConfig = SetupReqBuf.wValueL;
								break;
							case DEF_USB_CLR_FEATURE:
								if ( ( SetupReqBuf.bType & 0x1F ) == 0x02 ) {  // 不是端点不支持
									switch( SetupReqBuf.wIndexL ) {
										case 0x82:
											Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( Read374Byte( REG_USB_ENDP2 ) ) );
											break;
										case 0x02:
											Write374Byte( REG_USB_ENDP2, M_SET_EP2_RECV_ACK( Read374Byte( REG_USB_ENDP2 ) ) );
											break;
										case 0x81:
											Write374Byte( REG_USB_ENDP1, M_SET_EP1_TRAN_NAK( Read374Byte( REG_USB_ENDP1 ) ) );
											break;
										case 0x01:
											Write374Byte( REG_USB_ENDP1, M_SET_EP1_RECV_ACK( Read374Byte( REG_USB_ENDP1 ) ) );
											break;
										default:
											l = 0xFF;  // 操作失败
											break;
									}
								}
								else l = 0xFF;  // 操作失败
								break;
							case DEF_USB_GET_INTERF:
								Write374Byte( RAM_ENDP0_TRAN, 0 );
								if ( SetupLen >= 1 ) l = 1;
								break;
							case DEF_USB_GET_STATUS:
								Write374Byte( RAM_ENDP0_TRAN, 0 );
								Write374Byte( RAM_ENDP0_TRAN + 1, 0 );
								if ( SetupLen >= 2 ) l = 2;
								else l = SetupLen;
								break;
							default:
								l = 0xFF;  // 操作失败
								break;
						}
					}
				}
				else l = 0xFF;  // 操作失败
				if ( l == 0xFF ) {  // 操作失败
					Write374Byte( REG_USB_ENDP0, M_SET_EP0_RECV_STA( M_SET_EP0_TRAN_STA( 0 ) ) );  // STALL
				}
				else if ( l <= RAM_ENDP0_SIZE ) {  // 上传数据
					Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_ACK( M_SET_EP0_RECV_ACK( Read374Byte( REG_USB_ENDP0 ) ), l ) | BIT_EP0_TRAN_TOG );  // DATA1
				}
				else {  // 下传数据或其它
					Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( M_SET_EP0_RECV_ACK( Read374Byte( REG_USB_ENDP0 ) ) ) | BIT_EP0_RECV_TOG );  // DATA1
				}
				break;
			}
			case USB_INT_EP0_IN: {
				switch( SetupReq ) {
					case DEF_USB_GET_DESCR:
						l = SetupLen >= RAM_ENDP0_SIZE ? RAM_ENDP0_SIZE : SetupLen;  // 本次传输长度
						Write374Block( RAM_ENDP0_TRAN, l, pDescr );  /* 加载上传数据 */
						SetupLen -= l;
						pDescr += l;
						Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_ACK( Read374Byte( REG_USB_ENDP0 ), l ) ^ BIT_EP0_TRAN_TOG );
						break;
					case DEF_USB_SET_ADDRESS:
						Write374Byte( REG_USB_ADDR, SetupLen );
					default:
						Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( 0 ) );  // 结束
						break;
				}
				break;
			}
			case USB_INT_EP0_OUT: {
				switch( SetupReq ) {
//					case download:
//						get_data;
//						break;
					case DEF_USB_GET_DESCR:
					default:
						Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( 0 ) );  // 结束
						break;
				}
				break;
			}
			default: {
				break;
			}
		}
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_TRANSFER );  // 清中断标志
	}
	else if ( s & BIT_IF_USB_SUSPEND ) {  // USB总线挂起
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_USB_SUSPEND );  // 清中断标志
		Write374Byte( REG_SYS_CTRL, Read374Byte( REG_SYS_CTRL ) | BIT_CTRL_OSCIL_OFF );  // 时钟振荡器停止振荡,进入睡眠状态
	}
	else if ( s & BIT_IF_WAKE_UP ) {  // 芯片唤醒完成
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_WAKE_UP );  // 清中断标志
	}
	else {  // 意外的中断,不可能发生的情况,除了硬件损坏
		Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  // 清中断标志
	}
}

void	Init374Device( void )  // 初始化USB设备
{
	Write374Byte( REG_USB_ADDR, 0x00 );
	Write374Byte( REG_USB_ENDP0, M_SET_EP0_TRAN_NAK( 0 ) );
	Write374Byte( REG_USB_ENDP1, M_SET_EP1_TRAN_NAK( 0 ) );
	Write374Byte( REG_USB_ENDP2, M_SET_EP2_TRAN_NAK( 0 ) );
	Write374Byte( REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG );  // 清所有中断标志
	Write374Byte( REG_INTER_EN, BIT_IE_TRANSFER | BIT_IE_BUS_RESET | BIT_IE_USB_SUSPEND );  // 允许传输完成中断和USB总线复位中断以及USB总线挂起中断,芯片唤醒完成中断
	Write374Byte( REG_SYS_CTRL, BIT_CTRL_OE_POLAR );  // 对于CH374T或者UEN引脚悬空的CH374S必须置BIT_CTRL_OE_POLAR为1
	Write374Byte( REG_USB_SETUP, BIT_SETP_TRANS_EN | BIT_SETP_PULLUP_EN );  // 启动USB设备
}

int	main( void )  // USB device
{
//	P1&=0xF8; // 如果在U盘文件读写模块上试用本程序必须加上本行
	mDelaymS( 50 );  // 等待CH374复位完成
	CH374_PORT_INIT( );  /* CH374接口初始化 */
	Init374Device( );  // 初始化USB设备
	while ( 1 ) {
		if ( Query374Interrupt( ) ) USB_DeviceInterrupt( );  // 等待USB设备中断，然后处理USB设备中断
	}
}
