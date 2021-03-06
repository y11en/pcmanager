#include "StdAfx.h"
#include "ViewVulHandler.h"
#include <process.h>
#include <algorithm>
#include <functional>

#include "DlgMain.h"
#include "BeikeVulfixEngine.h"
#include "DlgIgnoredVuls.h"
#include "DlgInstalledVuls.h"

template<typename T>
int CountValidVuls(const CSimpleArray<LPTUpdateItem>& arr, T t)
{
	int count = 0;
	for(int i=0; i<arr.GetSize(); ++i)
	{
		LPTUpdateItem pItem = arr[i];
		if( !pItem->isIgnored && t(pItem->nWarnLevel) )
			++ count;
	}
	return count;
}

static int AppendItem2RepairList( CListViewCtrlEx &listCtrl, T_VulListItemData * pVulItem )
{
	CString strTitle, strFileSize;
	strTitle.Format(_T("KB%d"), pVulItem->nID);
	FormatSizeString(pVulItem->nFileSize, strFileSize);

	int nItem = listCtrl.Append(GetLevelDesc(pVulItem->nWarnLevel), false );
	listCtrl.AppendSubItem(nItem, strTitle);
	listCtrl.AppendSubItem(nItem, pVulItem->strName);
	listCtrl.AppendSubItem(nItem, strFileSize);
	listCtrl.AppendSubItem(nItem, pVulItem->nWarnLevel==-1 ? _T("无需修复") : _T("未修复"));
	listCtrl.AppendSubItem(nItem, _T("反馈问题"));
	listCtrl.SetItemData(nItem, (DWORD_PTR)pVulItem);
	return nItem;
}

CViewVulHandler::CViewVulHandler( CDlgMain &mainDlg ) : CBaseViewHandler<CDlgMain>(mainDlg)
{
	m_dwPos = 0;
	m_nCurrentVulFixTab = 0;
	m_nItemOfRightInfo = -1;
}

CViewVulHandler::~CViewVulHandler(void)
{
}

BOOL CViewVulHandler::Init()
{
	// vul list 
	m_wndListCtrlVul.Create( 
		GetViewHWND(), NULL, NULL, 
		//WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_OWNERDRAWFIXED | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_NOCOLUMNHEADER, 
		WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL , 
		0, 299, NULL);
	{
		m_wndListCtrlVul.InsertColumn(0, _T("选择"), LVCFMT_LEFT, 50);
		m_wndListCtrlVul.InsertColumn(1, _T("补丁名称"), LVCFMT_LEFT, 60);
		m_wndListCtrlVul.InsertColumn(2, _T("补丁描述"), LVCFMT_LEFT, 200);
		m_wndListCtrlVul.InsertColumn(3, _T("发布日期"), LVCFMT_LEFT, 80);
		m_wndListCtrlVul.InsertColumn(4, _T("状态"), LVCFMT_LEFT, 60);
	}
	
	// vul list 
	m_wndListCtrlVulFixing.Create( 
		GetViewHWND(), NULL, NULL, 
		//WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_OWNERDRAWFIXED | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_NOCOLUMNHEADER, 
		WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL , 
		0, 32001, NULL);

	{
		m_wndListCtrlVulFixing.InsertColumn(0, _T("重要"), LVCFMT_LEFT, 50);
		m_wndListCtrlVulFixing.InsertColumn(1, _T("补丁名称"), LVCFMT_LEFT, 80);
		m_wndListCtrlVulFixing.InsertColumn(2, _T("补丁描述"), LVCFMT_LEFT, 250);
		m_wndListCtrlVulFixing.InsertColumn(3, _T("补丁进度"), LVCFMT_LEFT, 100);
		m_wndListCtrlVulFixing.InsertColumn(4, _T("状态"), LVCFMT_LEFT, 100);
		m_wndListCtrlVulFixing.InsertColumn(5, _T("反馈问题"), LVCFMT_CENTER, 100);
	}

	m_ctlRichEdit.FirstInitialize(GetViewHWND(), 30450);
	m_ctlRichEdit.SetBackgroundColor( RGB(240, 244, 250) );
	return TRUE;
}


BOOL CViewVulHandler::OnBkTabVulFixSelChanged( int nTabItemIDOld, int nTabItemIDNew )
{
	m_nCurrentVulFixTab = nTabItemIDNew;
	_LoadTabContent(nTabItemIDNew);
	return TRUE;
}

LRESULT CViewVulHandler::OnListBoxVulFixNotify( int idCtrl, LPNMHDR pnmh, BOOL& bHandled )
{
	if(pnmh->code==LVN_ITEMCHANGED)
	{
		LPNMLISTVIEW pnmv = (LPNMLISTVIEW) pnmh; 
		DEBUG_TRACE(_T("ItemChanged (%d %d,%d,%d)\n"), pnmv->iItem, pnmv->uOldState, pnmv->uNewState, pnmv->uChanged);

		if( pnmv->uNewState & LVIS_SELECTED)
		{
			_DisplayRelateVulFixInfo( pnmv->iItem, m_nCurrentVulFixTab );
		}
	}
	bHandled = FALSE;
	return 0;
}

void CViewVulHandler::_DisplayRelateVulFixInfo( int nItem, int nCurrentTab )
{
	// Item Selected 
	T_VulListItemData *pItemData = NULL;
	if(nItem>=0)
		pItemData = (T_VulListItemData*)m_wndListCtrlVul.GetItemData( nItem );
	
	m_nItemOfRightInfo = nItem;
	if(pItemData)
	{
		SetRelateInfo(m_ctlRichEdit, pItemData, FALSE);
		
		SetItemVisible(302, FALSE);
		SetItemVisible(304, TRUE);
	}
	else
	{
		m_nItemOfRightInfo = -1; 
		SetItemVisible(304, FALSE);
		SetItemVisible(302, TRUE);
	}
}

BOOL CViewVulHandler::OnViewActived( INT nTabId, BOOL bActive )
{
	if(bActive)
	{
		_ReloadVulInfo();

	}
	return TRUE;
}

void CViewVulHandler::OnBkBtnSwitchRelateInfo()
{
	m_bRelateInfoShowing = !m_bRelateInfoShowing;
	if(m_bRelateInfoShowing)
	{
		SetItemText(30001, _T(">"), FALSE);
		SetItemVisible(301, TRUE);
		SetItemAttribute(300301, "pos", "5,35,-255,-5");
	}
	else
	{
		SetItemText(30001, _T("<"), FALSE);
		SetItemVisible(301, FALSE);
		SetItemAttribute(300301, "pos", "5,35,-5,-5");
	}

}

void CViewVulHandler::OnBkBtnIgnoredVuls()
{
	CDlgIgnoredVuls dlg;
	dlg.Load(IDR_BK_IGNORED_VUL_DIALOG);
	if( IDOK==dlg.DoModal() )
		OnBkBtnScan();
}

void CViewVulHandler::OnBkBtnInstalledVuls()
{
	CDlgInstalledVuls dlg;
	dlg.Load(IDR_BK_INSTALLED_VUL_DIALOG);
	dlg.DoModal();
}

LRESULT CViewVulHandler::OnVulFixEventHandle( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	int evt = uMsg - WMH_VULFIX_BASE;
	if(evt==EVulfix_ScanBegin||evt==EVulfix_ScanProgress)
	{
		if(evt==EVulfix_ScanBegin)
		{
			if(wParam==VTYPE_WINDOWS)
				m_nScanState = 0;
			else 
				++m_nScanState;
			m_nTotalItem = lParam;
			m_nCurrentItem = 0;
			int nPos = m_nScanState*100 + (m_nCurrentItem*100)/m_nTotalItem;
			_SetScanProgress(nPos);
		}
		else //EVulfix_ScanProgress:
		{
			if(lParam>(m_nCurrentItem+m_nTotalItem*0.05))
			{
				m_nCurrentItem = lParam;
				int nPos = m_nScanState*100 + (m_nCurrentItem*100)/m_nTotalItem;
				_SetScanProgress(nPos);
			}
		}
	}
	else
	{
		int nKbId = wParam;
		int nIndex = FindListItem( nKbId );
		if(nIndex==-1)
			return 0; 
		
		int nSubitem = -1;
		CString strTitle;
		
		switch (evt)
		{
		case EVulfix_DownloadBegin:
		case EVulfix_DownloadProcess:
		case EVulfix_DownloadEnd:
		case EVulfix_DownloadError:
			nSubitem = 3;
			if( EVulfix_DownloadProcess==evt )
			{
				T_VulListItemData *pItemData = (T_VulListItemData*)m_wndListCtrlVulFixing.GetItemData( nIndex );
				ATLASSERT(pItemData);
				if(pItemData)
				{
					CString strFileSize, strDownloadedSize;
					FormatSizeString(lParam, strDownloadedSize);
					FormatSizeString(pItemData->nFileSize, strFileSize);
					strTitle.Format(_T("%s/%s"), strDownloadedSize, strFileSize);
				}
			}
			else if(EVulfix_DownloadEnd==evt)
			{
				strTitle = _T("已下载");
				++m_nRepairDownloaded;
			}
			else if(EVulfix_DownloadError==evt)
			{
				strTitle = _T("下载失败 ");
				++ m_nRepairProcessed;
			}
			else
				strTitle = _T("正连接 ");
			
			break;
		
		case EVulfix_InstallBegin:
		case EVulfix_InstallEnd:
		case EVulfix_InstallError:
			nSubitem = 4;
			if(EVulfix_InstallBegin==evt)
				strTitle = _T("正安装 ");
			else if(EVulfix_InstallEnd==evt)
			{
				strTitle = _T("已安装");
				++m_nRepairInstalled;
			}
			else if(EVulfix_InstallError==evt)
				strTitle = _T("安装失败");

			if(EVulfix_InstallError==evt || EVulfix_InstallEnd==evt)
				++ m_nRepairProcessed;
			break;
		
		case EVulfix_Task_Complete:
		case EVulfix_Task_Error:
			break;
		}
		if(nSubitem>=0)
			m_wndListCtrlVulFixing.SetSubItem(nIndex, nSubitem, strTitle);
		if(evt==EVulfix_DownloadBegin || evt==EVulfix_DownloadError || evt==EVulfix_InstallEnd || evt==EVulfix_InstallError)
			// 更新Title 
		{
			_UpdateRepairTitle();
			_SetRepairProgress( m_nRepairTotal==0 ? 100 : (100*m_nRepairProcessed)/m_nRepairTotal );
		}
	}
	return 0;
}

int CViewVulHandler::FindListItem( int nID )
{
	static int nItem = -1;

	// if cache meet 
	if(nItem>=0 && nItem<m_wndListCtrlVulFixing.GetItemCount())
	{
		T_VulListItemData *pItemData = (T_VulListItemData *)m_wndListCtrlVulFixing.GetItemData(nItem);
		if(pItemData && pItemData->nID==nID)
			return nItem;
	}

	// find all 
	for(int i=0; i<m_wndListCtrlVulFixing.GetItemCount(); ++i)
	{
		T_VulListItemData *pItemData = (T_VulListItemData *)m_wndListCtrlVulFixing.GetItemData(i);
		if(pItemData && pItemData->nID==nID)
			return nItem = i;
	}
	return nItem = -1;
}

void CViewVulHandler::OnBkBtnSelectAll()
{
	SetListCheckedAll(m_wndListCtrlVul, true);
}

void CViewVulHandler::OnBkBtnSelectNone()
{
	SetListCheckedAll(m_wndListCtrlVul, false);
}


void CViewVulHandler::OnBkBtnIgnore()
{
	CSimpleArray<int> arr;
	if( GetListCheckedItems(m_wndListCtrlVul, arr) )
	{
		theEngine->IgnoreVuls(arr, true);
		OnBkBtnScan();
	}
}

void CViewVulHandler::OnBkBtnRepair()
{
	CSimpleArray<int> arr;
	if( GetListCheckedItems(m_wndListCtrlVul, arr) )
	{
		m_nRepairTotal = arr.GetSize();
		m_nRepairInstalled = 0;
		m_nRepairDownloaded = 0;
		m_nRepairProcessed = 0;

		m_RefWin.SetNoDisturb( TRUE, TRUE, 31380 );
		
		_UpdateRepairTitle();
		SetItemVisible(300, FALSE);
		SetItemVisible(310, TRUE);
		SCAN_STATE st = STATE_REPAIRING;
		_SetScanState(st);

		// Fill the list ctrl 
		ResetListCtrl(m_wndListCtrlVulFixing);
		for(int i=0; i<m_wndListCtrlVul.GetItemCount(); ++i)
		{
			if(m_wndListCtrlVul.GetCheckState(i))
			{
				T_VulListItemData *pItem = (T_VulListItemData*) m_wndListCtrlVul.GetItemData( i );
				AppendItem2RepairList( m_wndListCtrlVulFixing, new T_VulListItemData( *pItem ) );
			}
		}
		
		// Fix 
		CSimpleArray<int> arrSoftVul;
		theEngine->RepairAll( (HWND)-2, arr, arrSoftVul);
	}
}

void CViewVulHandler::_SetScanState( SCAN_STATE st )
{
	int baseid = 311;
	for(int i=0; i<STATE_END; ++i)
	{
		SetItemVisible(baseid+i, st==i);
	}
}

void CViewVulHandler::_UpdateRepairTitle()
{
	CString strTitle;
	strTitle.Format(_T("正在修复系统漏洞(%d, %d/%d)..."), m_nRepairInstalled, m_nRepairDownloaded, m_nRepairTotal );
	SetItemText(31300, strTitle);
}

void CViewVulHandler::_SetScanProgress( int nPos )
{
	SetItemDWordAttribute(31201, "value", nPos, TRUE);
}

void CViewVulHandler::_SetRepairProgress( int nPos )
{
	SetItemDWordAttribute(31301, "value", nPos, TRUE);
}

void CViewVulHandler::OnBkBtnScan()
{
	m_RefWin.SetNoDisturb( TRUE, FALSE, 31280 );
	theEngine->ScanVul( (HWND)-2 );

	SetItemVisible(300, FALSE);
	SetItemVisible(310, TRUE);
	_SetScanState(STATE_SCANNING);
}

void CViewVulHandler::OnBkBtnCancelScan()
{
	m_RefWin.SetNoDisturb( FALSE, FALSE );
	_SetScanState(STATE_CANCELED);
}

void CViewVulHandler::OnBkBtnCancelRepair()
{
	m_RefWin.SetNoDisturb( FALSE, FALSE );
	theEngine->CancelRepair();
	OnBkBtnScan();
}

void CViewVulHandler::OnBkBtnRunBackground()
{
	m_RefWin.RunBackGround();
}

void CViewVulHandler::OnBkBtnReboot()
{
	m_RefWin.ShutDownComputer(TRUE);
}

LRESULT CViewVulHandler::OnScanDone( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	SetItemVisible(300, TRUE);
	SetItemVisible(310, FALSE);
	_ReloadVulInfo();
	m_RefWin.SetNoDisturb( FALSE, FALSE );
	return 0;
}

LRESULT CViewVulHandler::OnRepaireDone( UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )
{
	m_RefWin.SetNoDisturb( FALSE, FALSE );
	if(m_nRepairInstalled==m_nRepairTotal)
	{
		_SetScanState(STATE_REPAIRDONE);
	}
	else if(m_nRepairInstalled>0)
	{
		_SetScanState(STATE_REPAIRDONE_PART);
	}
	else
	{
		_SetScanState(STATE_REPAIRERROR);	
	}
	return 0;
}

void CViewVulHandler::_ReloadVulInfo()
{
	const CSimpleArray<LPTUpdateItem>& arr = theEngine->m_pVulScan->GetResults();
	const CSimpleArray<TReplacedUpdate*>& arrReplaced = theEngine->m_pVulScan->GetReplacedUpdates();

	int nLeak = CountValidVuls( arr, std::bind2nd( std::greater<int>(), 0 ) );
	int nOptional = CountValidVuls( arr, std::bind2nd( std::less<int>(), 1 ) );
	
	CStringA strTitle;
	strTitle.Format( CT2A(_T("系统补丁(%d)"), CP_UTF8), nLeak);
	SetItemAttribute(300300, "title", strTitle, FALSE);

	strTitle.Format( CT2A(_T("可选补丁(%d)"), CP_UTF8), nOptional);
	SetItemAttribute(300301, "title", strTitle, FALSE);

	strTitle.Format( CT2A(_T("失效补丁(%d)"), CP_UTF8), arrReplaced.GetSize() );		
	SetItemAttribute(300302, "title", strTitle, TRUE);

	_LoadTabContent( m_nCurrentVulFixTab );
}

void CViewVulHandler::_LoadTabContent( int nTabItemIDNew )
{
	if(nTabItemIDNew==0)
	{
		_FillVulFixList( std::bind2nd(std::greater<int>(), 0) );
		SetListCheckedAll( m_wndListCtrlVul, true );

		SetItemVisible(305, TRUE);
		SetItemVisible(36000, FALSE);
		SetItemVisible(36001, FALSE);
	}
	else if(nTabItemIDNew==1)
	{
		_FillVulFixList( std::bind2nd(std::less<int>(), 1) );

		SetItemVisible(305, TRUE);
		SetItemVisible(36000, TRUE);
		SetItemVisible(36001, FALSE);
	}
	else if(nTabItemIDNew==2)
	{
		ResetListCtrl(m_wndListCtrlVul);
		const CSimpleArray<TReplacedUpdate*>& arr = theEngine->m_pVulScan->GetReplacedUpdates();
		for(int i=0; i<arr.GetSize(); ++i)
		{
			TReplacedUpdate* pItem = arr[i];
			AppendItem2List( m_wndListCtrlVul, CreateListItem(pItem) );
		}
		_DisplayRelateVulFixInfo(-1, m_nCurrentVulFixTab);

		SetItemVisible(305, FALSE);
		SetItemVisible(36000, FALSE);
		SetItemVisible(36001, TRUE);
	}
}

LRESULT CViewVulHandler::OnRichEditLink( int idCtrl, LPNMHDR pnmh, BOOL& bHandled )
{
	ENLINK *pLink = (ENLINK*)pnmh;
	if(pLink->msg==WM_LBUTTONUP)
	{
		// 点击了了解更多
		T_VulListItemData *pItemData = NULL;
		if(m_nItemOfRightInfo>=0)
			pItemData = (T_VulListItemData*)m_wndListCtrlVul.GetItemData( m_nItemOfRightInfo );
		if(pItemData)
		{
			ATLASSERT(!pItemData->strWebPage.IsEmpty());
			ShellExecute(NULL, _T("open"), pItemData->strWebPage, NULL, NULL, SW_SHOW);
		}
	}
	return 0;
}