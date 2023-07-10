#pragma once

// CDataSocket 명령 대상입니다.

class CUDPServer_thdDlg;


class CDataSocket : public CSocket
{
public:
	CDataSocket(CUDPServer_thdDlg *pDlg);
	virtual ~CDataSocket();
	CUDPServer_thdDlg *m_pDlg;
	virtual void OnReceive(int nErrorCode);
	virtual void OnClose(int nErrorCode);
};


