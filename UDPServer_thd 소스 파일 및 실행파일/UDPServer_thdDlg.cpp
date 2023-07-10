// UDPServer_thdDlg.cpp : 구현 파일
//

#include "stdafx.h"
#include "UDPServer_thd.h"
#include "UDPServer_thdDlg.h"
#include "afxdialogex.h"
#include "DataSocket.h"

#define CL_PORT_NUMBER 8100
#define IP_ADDR "127.0.0.1"
#define SV_PORT_NUMBER 8000
#define SEGMENTATION 8

bool Timer_Run = FALSE;
bool Timeout = FALSE;
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CCriticalSection tx_cs, rx_cs;

// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

														// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CUDPServer_thdDlg 대화 상자



CUDPServer_thdDlg::CUDPServer_thdDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_UDPSERVER_THD_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUDPServer_thdDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_tx_edit_short);
	DDX_Control(pDX, IDC_EDIT3, m_tx_edit);
	DDX_Control(pDX, IDC_EDIT2, m_rx_edit);
	this->m_pDataSocket = NULL;
}

BEGIN_MESSAGE_MAP(CUDPServer_thdDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_SEND, &CUDPServer_thdDlg::OnBnClickedSend)
	ON_BN_CLICKED(IDC_DISCONNECT, &CUDPServer_thdDlg::OnBnClickedDisconnect)
END_MESSAGE_MAP()



bool compare_checksum(Frame frm)
{
	unsigned short sum = 0; // 결과 더해줄 변수
	unsigned short checksum = 0;

	for (int i = 0; frm.buffer[i]; i++)
	{
		sum += frm.buffer[i];
	}

	checksum = (sum + frm.ms_length) + frm.checksum;
	if (checksum > 0x00010000)// 캐리가 발생한 경우
	{
		checksum = (checksum >> 16) + (checksum & 0x0000FFFF); //체크섬에 발생한 캐리를 더해준다
	}
	if ((0xFFFF - checksum) == 0)
	{
		return TRUE;
		//오류가 검출 되지 않고 정상적인 경우
	}
	else
		return FALSE;
	//오류가 검출된 경우
}

UINT TimerThread(LPVOID arg)
{
	ThreadArg *pArg = (ThreadArg*)arg;

	while (pArg->Thread_run)// 스레드가 돌아가면
	{
		/**/
		if (Timer_Run == TRUE) {// 타이머켜짐
			int SendOut = GetTickCount();// Tick값을 받아옴

			while (1) {
				if ((Timer_Run == FALSE)) {//시간내에 확인이 되면 재전송요구x 타이머종료.
					break;
				}
				if ((GetTickCount() - SendOut) / CLOCKS_PER_SEC > 10)//최대 대기 시간 10초
				{//시간초과시 재전송 요구
					Timeout = TRUE;
					break;
				}
			}
		}
		Sleep(100);
	}
	return 0;
}


UINT RXThread(LPVOID arg) // 받은 message 출력
{
	ThreadArg *pArg = (ThreadArg *)arg; // Thread 구조체포인터변수 선언
	CList <Frame> *plist = pArg->pList; // Frame형 구조체 배열 생성
	CUDPServer_thdDlg *pDlg = (CUDPServer_thdDlg*)pArg->pDlg; // 생성변수 객체 위치 저장

	Frame NFrame; // Frame 형으로 생성한 plist의 다음 원소를 저장하기 위한 변수
	CString Mid[16];// reassembly에 관한 변수
	CString Fullms;


	while (pArg->Thread_run)
	{
		POSITION pos = plist->GetHeadPosition();// 먼저 들어온 메시지 -> Head에 저장
		POSITION current_pos;

		while (pos != NULL) // message가 존재한다면
		{
			current_pos = pos;

			rx_cs.Lock();
			NFrame = plist->GetNext(pos);//message frame 버퍼에 넣기
			rx_cs.Unlock();

			//checksum
			if (compare_checksum(NFrame) == 0) // checksum부분에서 오류가 있다면 
			{
				AfxMessageBox(_T("checksum error!"));
			}
			else
			{
				CString sq_num;
				sq_num.Format(_T("%d"), NFrame.seq_num);// sequence number 받아옴
				if (NFrame.check == 0)// 재전송이 아닐때
				{
					AfxMessageBox(sq_num + _T("프레임 도착!"));
					NFrame.ack_num = TRUE;
					pDlg->m_pDataSocket->SendToEx(&NFrame, sizeof(Frame), pArg->ClientPort, pArg->ClientAddr);// 목적지로 ack 보냄
				}
				else
				{
					AfxMessageBox(sq_num + _T("번 프레임이 재전송 됨"));
				}
			}

			//재조립
			int ms_l = NFrame.ms_length;

			if ((NFrame.SourcePort == pArg->ClientPort))
			{
				if (NFrame.check == 0)// 재전송 아닐때
				{
					if (NFrame.seq_num == ms_l / SEGMENTATION)// sequence number가 마지막 번호의 프레임과 일치하면
					{
						Mid[NFrame.seq_num] = (LPCTSTR)NFrame.buffer;
						// 준비해둔 Mid배열에 sequence 번호 index에 프레임에 있는 나눠진 문자 담아줌
						for (int i = 0; i <= ms_l / SEGMENTATION; i++)// 프레임 개수만큼
						{
							Fullms += Mid[i];// 전체 문자에 Mid배열에 담긴 나눠진 문자들 더해줌
							Mid[i] = _T("");
						}
					}
					else// 아니면 도착한 프레임에 있는 문자들 그냥 담아줌
					{
						Mid[NFrame.seq_num] = (LPCTSTR)NFrame.buffer;
						plist->RemoveAt(current_pos);
						continue;
					}
				}
				else
				{
					Mid[NFrame.seq_num] = (LPCTSTR)NFrame.buffer;
					plist->RemoveAt(current_pos);
					continue;
				}
			}

			int len = pDlg->m_rx_edit.GetWindowTextLengthW();//화면전체 텍스트길이 불러옴
			pDlg->m_rx_edit.SetSel(len, len);
			pDlg->m_rx_edit.ReplaceSel(_T(" ") + Fullms);//내가 받아온 메세지를 rx화면에 출력
			Fullms = _T("");//프레임 재조립 한 것 =  완전한 메세지 담아놓은 버퍼 초기화
			plist->RemoveAt(current_pos);//수신과정이 완료-> 현재 위치를 초기화

		}
		Sleep(10);
	}
	return 0;
}

UINT TXThread(LPVOID arg) // message 전송
{
	ThreadArg *pArg = (ThreadArg*)arg;
	CList<Frame> *plist = pArg->pList; //plist
	CUDPServer_thdDlg *pDlg = (CUDPServer_thdDlg*)pArg->pDlg; //대화상자 가리키는 변수 생성

	Frame NFrame;// Frame 형으로 생성한 plist의 다음 원소를 저장하기 위한 변수

	while (pArg->Thread_run)
	{
		POSITION pos = plist->GetHeadPosition();// 헤더의 위치를 받아옴
		POSITION current_pos;
		while (pos != NULL)
		{
			current_pos = pos; 
			tx_cs.Lock(); //임계구역
			NFrame = plist->GetNext(pos); //list의 맨앞부분 가리키도록 위치설정
			tx_cs.Unlock();	//임계구역

			pDlg->m_pDataSocket->SendToEx(&NFrame, sizeof(Frame), pArg->ClientPort, pArg->ClientAddr);// 클라이언트로 Frame 보냄

			Timer_Run = TRUE;
			while (1) {
				if (Timeout == TRUE) {
					NFrame.check = 1;
					pDlg->m_pDataSocket->SendToEx(&NFrame, sizeof(Frame), pArg->ClientPort, pArg->ClientAddr);// 재전송
					Timeout = FALSE;

					int len = pDlg->m_tx_edit.GetWindowTextLengthW();
					CString message = _T("(TimeOut! retransmit the Frame...)\r\n");// 시간이 다되었다고 표시
					pDlg->m_tx_edit.SetSel(len, len);
					pDlg->m_tx_edit.ReplaceSel(message);

					Sleep(10);
					break;
				}
				if (Timer_Run == FALSE) {// 타이머가 다되면 while문 빠져나옴
					break;
				}
			}
			plist->RemoveAt(current_pos); //전송 후 현재 메세지 삭제
		}
		Sleep(10);
	}
	return 0;
}

BOOL CUDPServer_thdDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

									// TODO: 여기에 추가 초기화 작업을 추가합니다.

	m_pDataSocket = NULL; // 초기화

	CList <Frame>* newlist = new CList <Frame>;
	arg1.pList = newlist;
	arg1.Thread_run = 1;
	arg1.pDlg = this;

	CList <Frame>* newlist2 = new CList <Frame>;
	arg2.pList = newlist2;
	arg2.Thread_run = 1;
	arg2.pDlg = this;
	arg2.r_seq_num = -1;

	CList<Frame> * newlist3 = new CList<Frame>;
	arg3.pList = newlist3;
	arg3.Thread_run = 1;
	arg3.pDlg = this;
	arg3.r_seq_num = -1;


	WSADATA wsa; // 에러 발생시 실행
	int error_code;
	if ((error_code = WSAStartup(MAKEWORD(2, 2), &wsa)) != 0)
	{
		TCHAR buffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);
		AfxMessageBox(buffer, MB_ICONERROR);
	}

	m_pDataSocket = new CDataSocket(this);
	if (m_pDataSocket->Create(SV_PORT_NUMBER, SOCK_DGRAM))
	{
		AfxMessageBox(_T("서버를 시작합니다."), MB_ICONINFORMATION);
		pThread1 = AfxBeginThread(TXThread, (LPVOID)&arg1);
		pThread2 = AfxBeginThread(RXThread, (LPVOID)&arg2);
		pThread3 = AfxBeginThread(TimerThread, (LPVOID)&arg3);
		return TRUE;
	}


	else
	{
		int err = m_pDataSocket->GetLastError();
		TCHAR buffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);
		AfxMessageBox(buffer, MB_ICONERROR);
	}

	AfxMessageBox(_T("이미 실행 중인 서버가 있습니다.") _T("\n프로그램을 종료합니다."), MB_ICONERROR);

	return FALSE;
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 응용 프로그램의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.
void CUDPServer_thdDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}
void CUDPServer_thdDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CUDPServer_thdDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CUDPServer_thdDlg::OnBnClickedSend()
{
	if (arg2.ClientPort == -1) {
		MessageBox(_T("클라이언트에서 먼저 전송을 받고 전송을 해주세요"), _T("오류 !"), MB_ICONERROR);
		m_tx_edit_short.SetWindowText(_T(""));
		return;
	}
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	CString tx_message;
	Frame frm;
	frm.SourcePort = SV_PORT_NUMBER;
	m_tx_edit_short.GetWindowText(tx_message); // message 받아오기


	frm.ms_length = tx_message.GetLength();

	if ((frm.ms_length / SEGMENTATION) == 0) // 프레임에 친 단어가 넘치지 않는다면
	{
		tx_message += _T("\r\n");
		_tcscpy_s(frm.buffer, (LPCTSTR)tx_message);

		frm.seq_num = 0;
		frm.checksum = checksum_result(frm);// 체크섬계산
		arg1.r_seq_num = 0;

		tx_cs.Lock();//임계구역 진입
		arg1.pList->AddTail(frm);// 보내는 리스트에 프레임 실어보냄
		arg3.pList->AddTail(frm); // timer
		tx_cs.Unlock();//임계구역 해제
	}
	else // 만약 프레임에 친 단어가 넘친다면
	{
		int i = 0;
		for (i = 0; i < (frm.ms_length / SEGMENTATION); i++) // 사용해야할 프레임 수 만큼
		{
			_tcscpy_s(frm.buffer, (LPCTSTR)tx_message.Mid(i * SEGMENTATION, SEGMENTATION));
			// 프레임에 데이터 담을 공간에 있는 문장 가져옴 , ex) i = 0일때 0부터 8개문자가 들어감
			frm.seq_num = i;// 프레임의 번호 달아줌
			frm.checksum = checksum_result(frm);// 프레임의 체크섬 계산

			tx_cs.Lock();//임계구역 잠금
			arg1.pList->AddTail(frm); arg1.pList->AddTail(frm); // 보낼 리스트에 프레임 넣어줌
			arg3.pList->AddTail(frm);// timer
			tx_cs.Unlock();//임계구역 해제
		}
		tx_message += _T("\r\n");//받아온 메세지에 줄바꿈 문자를 추가해줌
		CString EndOfString = tx_message.Mid(i * SEGMENTATION, (frm.ms_length - i*SEGMENTATION) + 3);
		// i*SEGMENTATION에서 부터 frm길이 - i*SEGMENTATION + 3개의 문자 저장
		_tcscpy_s(frm.buffer, EndOfString);// frame에 가득 채우고 남은 문자 저장
		frm.seq_num = i;// 프레임 번호 i
		frm.checksum = checksum_result(frm);// 체크섬
		arg1.r_seq_num = i;// 최근 프레임 number를 i로 설정

		tx_cs.Lock();//임계구역 잠금
		arg1.pList->AddTail(frm);
		arg3.pList->AddTail(frm);
		tx_cs.Unlock();//임계구역 해제

	}//SEGMENTATION
	m_tx_edit_short.SetWindowText(_T(""));//초기화
	m_tx_edit_short.SetFocus();

	int len = m_tx_edit.GetWindowTextLengthW();//화면전체 텍스트길이 불러옴
	m_tx_edit.SetSel(len, len);// 문자를 입력 했을때 커서의 위치를 끝으로 보내기 위함
	m_tx_edit.ReplaceSel(_T("") + tx_message);//내가 전송한 메세지를 전송창에 출력
}


void CUDPServer_thdDlg::OnBnClickedDisconnect() // Disconnect 하기 위함
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	if (m_pDataSocket == NULL)
	{
		AfxMessageBox(_T("이미 접속 종료"));
	}
	else
	{
		arg1.Thread_run = 0;
		arg2.Thread_run = 0;
		m_pDataSocket->Close();
		delete m_pDataSocket;
		m_pDataSocket = NULL;
	}
}

unsigned short CUDPServer_thdDlg::checksum_result(Frame send_frame)
{
	unsigned short sum = 0;
	unsigned short checksum = 0; // checksum으로 사용할 변수

	for (int i = 0; send_frame.buffer[i]; i++) {
		sum += send_frame.buffer[i];// 모든 문자를 추가해주기 위함
	}

	checksum = sum + send_frame.ms_length;
	if (checksum > 0x00010000)// 캐리가 발생한 경우
	{
		checksum = (checksum >> 16) + (checksum & 0x0000FFFF); // 체크섬에 발생한 캐리를 더해줌
	}

	checksum = ~(unsigned short)(checksum);
	return checksum;
}



void CUDPServer_thdDlg::ProcessClose(CDataSocket* pSocket, int nErrorCode)
{
	arg1.Thread_run = 0;
	arg2.Thread_run = 0;
	pSocket->Close(); // 종료
	delete m_pDataSocket;
	m_pDataSocket = NULL;

	int len = m_rx_edit.GetWindowTextLengthW();
	CString message = _T("### 접속 종료 ###\r\n\r\n");
	m_rx_edit.SetSel(len, len);
	m_rx_edit.ReplaceSel(message);
}


void CUDPServer_thdDlg::ProcessReceive(CDataSocket* pSocket, int nErrorCode)
{
	Frame frm;
	UINT PeerPort;
	CString PeerAddr;

	if (pSocket->ReceiveFromEx(&frm, (sizeof(Frame)), PeerAddr, PeerPort) < 0)//UDP 정보를 수신했는데 오류가 있을때
	{
		AfxMessageBox(_T("Receive Error"), MB_ICONERROR);
	}

	if (frm.ack_num == FALSE) {//ack가 안왔으면
		rx_cs.Lock();
		arg2.pList->AddTail(frm);//CString타입의 수신 데이터를 rx대기리스트에 추가함
		arg2.ClientPort = PeerPort;// 받아온 주소
		arg2.ClientAddr = (LPCTSTR)PeerAddr;// 받아온 port번호 rx대기리스트에 추가
		rx_cs.Unlock();

		rx_cs.Lock();
		arg1.ClientAddr = (LPCTSTR)PeerAddr;
		arg1.ClientPort = PeerPort;
		rx_cs.Unlock();
	}
	else if ((frm.ack_num == TRUE)) {// ack가 왔으면
		Timer_Run = FALSE;// 타이머 종료
	}
}
