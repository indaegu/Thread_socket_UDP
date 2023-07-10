
// UDPClient_thdDlg.h : 헤더 파일
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "DataSocket.h"

typedef struct
{

	TCHAR buffer[256]; // 전송할 메세지를 담아 주기 위한 변수
	int seq_num = -1; // sequence number로 프레임 번호를 지정해줌
	int ms_length = 0; // message의 길이를 설정
	unsigned short checksum = 0; // 체크섬 변수 설정
	bool ack_num = FALSE; // ack 넘버를 지정해줌 0,1 두개로 2bit로 연산 해줌
	UINT SourcePort;
	CString SourceAddress;
	int check = 0;// check가 0이면 pass, 1이면 retransmit


}Frame;
struct ThreadArg
{
	CList <Frame>*pList;
	CDialogEx* pDlg;
	int Thread_run;
	int r_seq_num;
	UINT Dst_Port = -1;
	CString Dst_Addr;
};

class CDataSocket;

// CUDPClient_thdDlg 대화 상자
class CUDPClient_thdDlg : public CDialogEx
{
	// 생성입니다.
public:
	CUDPClient_thdDlg(CWnd* pParent = NULL);	// 표준 생성자입니다.

												// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_UDPCLIENT_THD_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


														// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnEnChangeEdit1();
	CWinThread*pThread1, *pThread2, *pThread3; // TX, RX, TIMER 스레드에 사용될 변수
	ThreadArg arg1, arg2, arg3;
	CDataSocket*m_pDataSocket;
	CIPAddressCtrl m_ipaddr;
	CEdit m_tx_edit_short; // 전송창 중 입력창
	CEdit m_tx_edit; // 전송창
	CEdit m_rx_edit;// 수신창
	afx_msg void OnBnClickedSend();
	afx_msg void OnBnClickedClose();
	unsigned short checksum_result(Frame);
	void ProcessClose(CDataSocket* pSocket, int nErrorCode);
	void ProcessReceive(CDataSocket* pSocket, int nErrorCode);
};
