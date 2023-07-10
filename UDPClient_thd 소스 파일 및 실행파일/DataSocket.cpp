#include "stdafx.h"
#include "UDPClient_thd.h"
#include "DataSocket.h"
#include "UDPClient_thdDlg.h"


CDataSocket::CDataSocket(CUDPClient_thdDlg*pDlg)
{
	m_pDlg = pDlg;
}

CDataSocket::~CDataSocket()
{
}

void CDataSocket::OnReceive(int nErrorCode)
{
	CSocket::OnReceive(nErrorCode);
	m_pDlg->ProcessReceive(this, nErrorCode);
}

